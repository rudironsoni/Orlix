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

Net::HTTP.start(uri.host, uri.port, use_ssl: true) do |http|
  request = Net::HTTP::Get.new(uri)
  http.request(request) do |response|
    unless response.is_a?(Net::HTTPSuccess) || response.is_a?(Net::HTTPRedirection)
      abort "Bazel download failed: HTTP #{response.code}"
    end
    if response.is_a?(Net::HTTPRedirection)
      redirect = URI(response.fetch("location"))
      Net::HTTP.start(redirect.host, redirect.port, use_ssl: true) do |redirect_http|
        redirect_http.request(Net::HTTP::Get.new(redirect)) do |redirect_response|
          abort "Bazel download redirect failed: HTTP #{redirect_response.code}" unless redirect_response.is_a?(Net::HTTPSuccess)
          temporary.open("wb") { |file| redirect_response.read_body { |chunk| file.write(chunk) } }
        end
      end
    else
      temporary.open("wb") { |file| response.read_body { |chunk| file.write(chunk) } }
    end
  end
end

actual = Digest::SHA256.file(temporary).hexdigest
abort "Bazel checksum mismatch: expected #{expected}, got #{actual}" unless actual == expected

temporary.chmod(0o755)
FileUtils.mv(temporary, binary)
puts binary
