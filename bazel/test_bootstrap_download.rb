require "minitest/autorun"
require "net/http"
require "pathname"
require "stringio"
require "tmpdir"

require_relative "bootstrap"

class TestBootstrapDownload < Minitest::Test
  FakeResponse = Struct.new(:code, :kind, :location, :body) do
    def is_a?(klass)
      return kind == :success if klass == Net::HTTPSuccess
      return kind == :redirect if klass == Net::HTTPRedirection
      super
    end

    def fetch(key)
      raise KeyError, key unless key == "location"

      location
    end

    def read_body
      yield body
    end
  end

  class FakeHTTP
    def self.queue=(queue)
      @queue = queue
    end

    def self.queue
      @queue
    end

    def self.start(_host, _port, use_ssl:)
      yield new
    end

    def request(_request)
      item = self.class.queue.shift
      raise item if item.is_a?(Exception)

      yield item
    end
  end

  def test_retries_http_502_then_writes_the_body
    destination = Pathname.new(Dir.mktmpdir).join("bazel")
    FakeHTTP.queue = [
      FakeResponse.new("502", :failure, nil, ""),
      FakeResponse.new("200", :success, nil, "bazel-bytes")
    ]
    OrlixBazelBootstrap.new(http: FakeHTTP).download(URI("https://example.test/bazel"), destination, pause: 0)
    assert_equal "bazel-bytes", destination.read
  end

  def test_404_is_not_retried
    destination = Pathname.new(Dir.mktmpdir).join("bazel")
    FakeHTTP.queue = [
      FakeResponse.new("404", :failure, nil, ""),
      FakeResponse.new("200", :success, nil, "should-not-run")
    ]
    error = assert_raises(OrlixBazelBootstrap::DownloadFailure) do
      OrlixBazelBootstrap.new(http: FakeHTTP).download(URI("https://example.test/bazel"), destination, pause: 0)
    end
    assert_match "HTTP 404", error.message
    refute destination.file?
  end

  def test_retries_a_dropped_connection
    destination = Pathname.new(Dir.mktmpdir).join("bazel")
    FakeHTTP.queue = [
      Errno::ECONNRESET.new,
      FakeResponse.new("200", :success, nil, "after-reset")
    ]
    OrlixBazelBootstrap.new(http: FakeHTTP).download(URI("https://example.test/bazel"), destination, pause: 0)
    assert_equal "after-reset", destination.read
  end
end
