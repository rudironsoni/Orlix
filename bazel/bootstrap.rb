require "digest"
require "fileutils"
require "net/http"
require "pathname"
require "rbconfig"
require "uri"

VERSION = "9.2.0"
CHECKSUMS = {
  "darwin-arm64" => "dd466352a3e4d3581b8898740ee1ff208866ccbe25f8d367c5dcb950219587e6",
  "darwin-x86_64" => "14c9bcb01303b38192e0e2895051c1bcf19bf89d7e416f5aeeeb48b6b624cfbf",
  "linux-arm64" => "049dd21f40ad979db11c3ee68c96a42ce75f1185e69ac61ab20de1501427a410",
  "linux-x86_64" => "7668a95db1250f12c40407251e4e203b4ec8bf39bc495d2f485b2d8c99048694"
}.freeze

class OrlixBazelBootstrap
  RETRYABLE_STATUS = %w[502 503 504].freeze
  RETRYABLE_ERRORS = [
    EOFError,
    IOError,
    SocketError,
    Timeout::Error,
    Errno::ECONNRESET,
    Errno::ECONNREFUSED,
    Errno::ETIMEDOUT,
    Errno::EPIPE
  ].freeze
  MAX_ATTEMPTS = 4

  class RetryableDownload < StandardError; end
  class DownloadFailure < StandardError; end

  def initialize(http: Net::HTTP)
    @http = http
  end

  def download(uri, destination, attempts: MAX_ATTEMPTS, pause: 1)
    failure = nil
    attempts.times do |index|
      begin
        write_once(uri, destination)
        return
      rescue RetryableDownload => error
        failure = error
        sleep(pause * (2**index)) if pause.positive? && index < attempts - 1
      end
    end
    raise failure
  end

  def write_once(uri, destination)
    start(uri) do |response|
      if response.is_a?(Net::HTTPRedirection)
        redirect = URI(response.fetch("location"))
        start(redirect) do |redirect_response|
          write_success(redirect_response, destination, "Bazel download redirect failed")
        end
      else
        write_success(response, destination, "Bazel download failed")
      end
    end
  rescue *RETRYABLE_ERRORS => error
    raise RetryableDownload, "Bazel download failed: #{error.class}: #{error.message}"
  end

  private

  def start(uri)
    @http.start(uri.host, uri.port, use_ssl: uri.scheme == "https") do |http|
      http.request(Net::HTTP::Get.new(uri)) do |response|
        yield response
      end
    end
  end

  def write_success(response, destination, prefix)
    code = response.code.to_s
    raise RetryableDownload, "#{prefix}: HTTP #{code}" if RETRYABLE_STATUS.include?(code)
    raise DownloadFailure, "#{prefix}: HTTP #{code}" unless response.is_a?(Net::HTTPSuccess)

    destination.open("wb") { |file| response.read_body { |chunk| file.write(chunk) } }
  end
end

if $PROGRAM_NAME == __FILE__
  host_os = RbConfig::CONFIG.fetch("host_os")
  host_cpu = RbConfig::CONFIG.fetch("host_cpu")
  os = host_os.include?("darwin") ? "darwin" : host_os.include?("linux") ? "linux" : nil
  cpu = %w[arm64 aarch64].include?(host_cpu) ? "arm64" : %w[x86_64 amd64].include?(host_cpu) ? "x86_64" : nil
  abort "unsupported Bazel bootstrap host: #{host_os} #{host_cpu}" unless os && cpu

  platform = "#{os}-#{cpu}"
  expected = CHECKSUMS.fetch(platform)
  install_root = Pathname.new(ENV.fetch("ORLIX_BAZEL_TOOL_ROOT", File.join(Dir.home, "Library", "Caches", "Orlix", "Tools", "bazel")))
  install_dir = install_root.join(VERSION)
  binary = install_dir.join("bazel")

  if binary.file? && Digest::SHA256.file(binary).hexdigest == expected
    puts binary
    exit 0
  end

  install_dir.mkpath
  asset = "bazel-#{VERSION}-#{platform}"
  uri = URI("https://github.com/bazelbuild/bazel/releases/download/#{VERSION}/#{asset}")
  temporary = install_dir.join(".#{asset}.download-#{Process.pid}")

  begin
    OrlixBazelBootstrap.new.download(uri, temporary)
  rescue OrlixBazelBootstrap::RetryableDownload, OrlixBazelBootstrap::DownloadFailure => error
    abort error.message
  end

  actual = Digest::SHA256.file(temporary).hexdigest
  abort "Bazel checksum mismatch: expected #{expected}, got #{actual}" unless actual == expected

  temporary.chmod(0o755)
  FileUtils.mv(temporary, binary)
  puts binary
end
