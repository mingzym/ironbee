require '../../clipp/clipp_test'

require 'fileutils'

# Integration testing.
class TestXRules < Test::Unit::TestCase
  include CLIPPTest

  def do_clipp_test(action, flag_to_check)
    clipp(
      :input_hashes => [
        simple_hash("GET / HTTP/1.1\nHost: foo.bar\n\n")
      ],
      :config => '''
        LoadModule ibmod_xrules.so
      ''',
      :default_site_config => <<-EOS
        XRuleIpv4 "0.0.0.0/0" #{action}
        Rule #{flag_to_check} @clipp_print #{flag_to_check} id:1 rev:1 phase:RESPONSE_HEADER
      EOS
    )
  end

  [
    [ "EnableBlockingMode",              "FLAGS:blockingMode",          1 ],
    [ "DisableBlockingMode",             "FLAGS:blockingMode",          0 ],
    [ "EnableRequestHeaderInspection",   "FLAGS:inspectRequestHeader",  1 ],
    [ "DisableRequestHeaderInspection",  "FLAGS:inspectRequestHeader",  0 ],
    [ "EnableRequestURIInspection",      "FLAGS:inspectRequestUri",     1 ],
    [ "DisableRequestURIInspection",     "FLAGS:inspectRequestUri",     0 ],
    [ "EnableRequestParamInspection",    "FLAGS:inspectRequestParams",  1 ],
    [ "DisableRequestParamInspection",   "FLAGS:inspectRequestParams",  0 ],
    [ "EnableRequestBodyInspection",     "FLAGS:inspectRequestBody",    1 ],
    [ "DisableRequestBodyInspection",    "FLAGS:inspectRequestBody",    0 ],
    [ "EnableResponseHeaderInspection",  "FLAGS:inspectResponseHeader", 1 ],
    [ "DisableResponseHeaderInspection", "FLAGS:inspectResponseHeader", 0 ],
    [ "EnableResponseBodyInspection",    "FLAGS:inspectResponseBody",   1 ],
    [ "DisableResponseBodyInspection",   "FLAGS:inspectResponseBody",   0 ],
  ].each do |testspec|

    action, flag_to_check, flag_value = testspec

    self.class_eval(
      <<-EOS
        def test_xrules_#{action}
          do_clipp_test("#{action}", "#{flag_to_check}")
          assert_no_issues
          assert_log_match /\\\[#{flag_to_check}\\\]: #{flag_value}/
        end
      EOS
    )
  end

  def test_xruleipv4_no_subnet
    clipp(
      modules: %w{ xrules },
      default_site_config: <<-EOS
        XRuleIpv4 "0.0.0.0" EnableBlockingMode
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_no_issues
  end

  def test_xruleipv6_no_subnet
    clipp(
      modules: %w{ xrules },
      default_site_config: <<-EOS
        XRuleIpv6 "::1" EnableBlockingMode
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_no_issues
  end

  def test_setblockflag
    clipp(
      modules: %w{ xrules txdump },
      config: '''
        TxDump TxFinished stdout Basic Flags
      ''',
      default_site_config: <<-EOS
        XRulePath "/" block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = On'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = On'
    assert_log_match 'IsBlocked'

  end

  def test_event_tags
    clipp(
      modules: %w{ xrules txdump },
      config: '''
        TxDump TxFinished stdout Flags
      ''',
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @imatch get \\
          id:1                          \\
          phase:REQUEST                 \\
          msg:woops                     \\
          event:alert                   \\
          tag:tag1                      \\
          tag:tag2                      \\
          tag:tag3
        XRuleEventTag a tag2 c tag3 block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = On'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = On'
  end

  def test_exception_two_tags_fail
    clipp(
      modules: %w{ xrules txdump },
      config: '''
        TxDump TxFinished stdout Flags
      ''',
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @imatch get \\
          id:1                          \\
          phase:REQUEST                 \\
          msg:woops                     \\
          event:alert                   \\
          tag:tag1                      \\
          tag:tag2                      \\
          tag:tag3
        XRuleException EventTag:a EventTag:tag2 block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = Off'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = Off'
  end

  def test_exception_two_tags_pass
    clipp(
      modules: %w{ xrules txdump },
      config: '''
        TxDump TxFinished stdout Flags
      ''',
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @imatch get \\
          id:1                          \\
          phase:REQUEST                 \\
          msg:woops                     \\
          event:alert                   \\
          tag:tag1                      \\
          tag:tag2                      \\
          tag:tag3
        XRuleException EventTag:tag1 EventTag:tag2 block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = On'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = On'
  end

  def test_exception_two_tags_and_net_ip_pass
    clipp(
      modules: %w{ xrules txdump },
      config: '''
        TxDump TxFinished stdout Flags
      ''',
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @imatch get \\
          id:1                          \\
          phase:REQUEST                 \\
          msg:woops                     \\
          event:alert                   \\
          tag:tag1                      \\
          tag:tag2                      \\
          tag:tag3
        XRuleException EventTag:tag1 EventTag:tag2 IPv4:5.6.7.0/24 block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = On'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = On'
  end

  def test_exception_two_tags_and_host_ip_pass
    clipp(
      modules: %w{ xrules txdump },
      config: '''
        TxDump TxFinished stdout Flags
      ''',
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @imatch get \\
          id:1                          \\
          phase:REQUEST                 \\
          msg:woops                     \\
          event:alert                   \\
          tag:tag1                      \\
          tag:tag2                      \\
          tag:tag3
        XRuleException EventTag:tag1 EventTag:tag2 IPv4:5.6.7.8 block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = On'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = On'
  end

  def test_exception_ip_path_tag_two_events
    auditlog_base_dir = File.join(BUILDDIR, "auditlogs")
    auditlog_idx      = File.join(auditlog_base_dir, "idx")
    FileUtils.rm_rf(auditlog_base_dir)
    FileUtils.mkdir_p(auditlog_base_dir)
    clipp(
      modules: %w{ xrules txdump },
      config: """
        TxDump TxFinished stdout Flags
        AuditLogBaseDir #{auditlog_base_dir}
        AuditLogIndex #{auditlog_idx}
        XRuleGenerateEvent on
      """,
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @imatch get \\
          id:1                          \\
          phase:REQUEST                 \\
          msg:woops                     \\
          event:alert                   \\
          tag:tag1                      \\
          block:advisory
        Rule REQUEST_METHOD @imatch get \\
          id:2                          \\
          phase:REQUEST                 \\
          msg:uhoh                      \\
          event:alert                   \\
          tag:tag2                      \\
          block:advisory
        XRuleException Path:/ EventTag:tag1 EventTag:tag2 IPv4:5.6.7.8 block
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /hi HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_log_match '"Block: Advisory" = On'
    assert_log_match '"Block: Phase" = Off'
    assert_log_match '"Block: Immediate" = On'
  end
end
