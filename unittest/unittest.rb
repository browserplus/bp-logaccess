#!/usr/bin/env ruby

require File.join(File.dirname(File.dirname(File.expand_path(__FILE__))),
                  'external/dist/share/service_testing/bp_service_runner.rb')
require 'uri'
require 'test/unit'
require 'open-uri'
require 'rbconfig'
include Config
require 'webrick'
include WEBrick

class TestLogAccess < Test::Unit::TestCase
  def setup
    subdir = 'build/LogAccess'
    if ENV.key?('BP_OUTPUT_DIR')
      subdir = ENV['BP_OUTPUT_DIR']
    end
    @cwd = File.dirname(File.expand_path(__FILE__))
    @service = File.join(@cwd, "../#{subdir}")
    nulldevice = "/dev/null"
    if CONFIG['arch'] =~ /mswin|mingw/
      nulldevice = "NUL"
    end
    @server = HTTPServer.new(:Port => 0,
                             :Logger => WEBrick::Log.new(nulldevice),
                             :AccessLog => [nil],
                             :BindAddress => "127.0.0.1")
    @urlLocal = "http://localhost:#{@server[:Port]}/"
    @urlFake = "http://www.yahoo.com/fake.html"
  end
  
  def teardown
  end

  def test_load_service
    BrowserPlus.run(@service) { |s|
    }
  end

  # BrowserPlus.LogAccess.get({params}, function{}())
  # Returns a list in "files" of filehandles associated with BrowserPlus logfiles.
  def test_bpnpapi_1
    BrowserPlus::Service.new(@service) { |s|
      i = s.allocate(@urlLocal)
# NEEDSWORK!!!  get() is returning PermissionDenied
      x = i.get()
      x = x[0].split('2')
      want = ENV["HOME"] + "/Library/Application Support/Yahoo!/BrowserPlus/"
      got = x[0]
      assert_equal(want, got)
      s.shutdown()
    }
  end

  def test_bpnpapi_2
    BrowserPlus::Service.new(@service) { |s|
      i = s.allocate(@urlLocal)
# NEEDSWORK!!!  get() is returning PermissionDenied
      x = i.get()
      x = x[0].split('/')
      want = "bpnpapi.log"
      got = x[x.size() - 1]
      assert_equal(want, got)
      s.shutdown()
    }
  end

  def test_BrowserPlusCore_1
    BrowserPlus::Service.new(@service) { |s|
      i = s.allocate(@urlLocal)
# NEEDSWORK!!!  get() is returning PermissionDenied
      x = i.get()
      x = x[1].split('2')
      want = ENV["HOME"] + "/Library/Application Support/Yahoo!/BrowserPlus/"
      got = x[0]
      assert_equal(want, got)
      s.shutdown()
    }
  end

  def test_BrowserPlusCore_2
    BrowserPlus::Service.new(@service) { |s|
      i = s.allocate(@urlLocal)
# NEEDSWORK!!!  get() is returning PermissionDenied
      x = i.get()
      x = x[1].split('/')
      want = "BrowserPlusCore.log"
      got = x[x.size() - 1]
      assert_equal(want, got)
      s.shutdown()
    }
  end

  def test_fakeurl
    BrowserPlus::Service.new(@service) { |s|
      i = s.allocate(@urlFake)
      assert_raise(RuntimeError) { x = i.get() }
      s.shutdown()
    }
  end
end
