require "digest"
require "json"
require "pathname"
require "yaml"

ROOT = Pathname.new(__dir__).join("../..").cleanpath.freeze
OUTPUT_ROOT = ROOT.join("bazel/migration").freeze
MAKEFILES = (%w[
  Makefile
  Orlix/Makefile
  OrlixCoreUtils/Makefile
  OrlixHostAdapter/Makefile
  OrlixKernel/Makefile
  OrlixMLibC/Makefile
  OrlixOS/Makefile
] + ROOT.join("make").children.select { |path| path.file? && path.extname == ".mk" }.map { |path| path.relative_path_from(ROOT).to_s }.sort).freeze

def sha256(path)
  Digest::SHA256.file(path).hexdigest
end

def normalize_yaml(value)
  case value
  when Hash
    value.each_with_object({}) do |(key, child), normalized|
      normalized[key == true ? "on" : key.to_s] = normalize_yaml(child)
    end
  when Array
    value.map { |child| normalize_yaml(child) }
  else
    value
  end
end

def yaml_file(path)
  data = begin
    YAML.load_file(path, aliases: true)
  rescue ArgumentError
    YAML.load_file(path)
  end
  normalize_yaml(data)
end

def logical_make_lines(path)
  result = []
  current = +""
  start_line = nil
  path.each_line.with_index(1) do |line, number|
    if current.empty?
      current = line
      start_line = number
    else
      current << line
    end
    next if current.rstrip.end_with?("\\")

    result << [start_line, current]
    current = +""
    start_line = nil
  end
  result << [start_line, current] unless current.empty?
  result
end

def make_targets(relative_path)
  path = ROOT.join(relative_path)
  phony = []
  logical_make_lines(path).each do |_line, text|
    next unless text.start_with?(".PHONY:")

    phony.concat(text.delete_suffix("\n").split(":", 2).last.gsub("\\", " ").split)
  end

  targets = []
  logical_make_lines(path).each do |line, text|
    next if text.start_with?("\t", " ")
    next unless text.include?(":")
    next if text.match?(/^[A-Za-z0-9_.-]+\s*[:+?]?=/)

    lhs, rhs = text.delete_suffix("\n").split(":", 2)
    next if lhs.nil? || rhs.nil?

    lhs.gsub("\\", " ").split.each do |name|
      next if name.empty? || name == ".PHONY" || name == ".DEFAULT_GOAL"
      next if name.start_with?("ifeq", "ifneq", "ifdef", "ifndef")

      classification = if name.start_with?("__")
        "private"
      elsif name.include?("%") || name.include?("$(") || name.include?("${") || name.include?("/")
        "generated-or-pattern"
      elsif phony.include?(name)
        "command"
      else
        "file-or-internal"
      end
      targets << {
        "name" => name,
        "source" => relative_path,
        "line" => line,
        "classification" => classification,
        "phony" => phony.include?(name),
        "prerequisites" => rhs.gsub("\\", " ").strip.split
      }
    end
  end
  targets
end

def legacy_target_map
  targets = MAKEFILES.flat_map { |path| make_targets(path) }
  {
    "schema" => 1,
    "purpose" => "Exhaustive explicit target inventory for the top-level and component Orlix Makefiles.",
    "source_files" => MAKEFILES.to_h { |path| [path, sha256(ROOT.join(path))] },
    "targets" => targets.sort_by { |target| [target.fetch("source"), target.fetch("line"), target.fetch("name")] }
  }
end

def xcode_target_map
  path = ROOT.join("project.yml")
  project = yaml_file(path)
  {
    "schema" => 1,
    "purpose" => "Resolved XcodeGen target, scheme, package, setting, source, resource, dependency, and build-phase inventory.",
    "source_file" => "project.yml",
    "source_sha256" => sha256(path),
    "project" => project
  }
end

def workflow_map
  paths = ROOT.join(".github/workflows").children.select { |path| path.file? && %w[.yml .yaml].include?(path.extname) }.sort
  workflows = paths.to_h do |path|
    relative = path.relative_path_from(ROOT).to_s
    [relative, {
      "source_sha256" => sha256(path),
      "workflow" => yaml_file(path)
    }]
  end
  {
    "schema" => 1,
    "purpose" => "Resolved workflow trigger, permission, environment, job, step, action, artifact, and release-operation inventory.",
    "workflows" => workflows
  }
end

def proof_owner(target)
  name = target.fetch("name")
  return "OrlixTCTI" if name.include?("tcti")
  return "OrlixMLibC" if name.include?("mlibc")
  return "OrlixCoreUtils" if name.include?("coreutils")
  return "OrlixHostAdapter" if name.include?("hostadapter")
  return "OrlixOS" if name.include?("orlixos") || name.include?("runtime") || name.include?("console")
  return "Orlix" if name.include?("app") || name.include?("beta") || name.include?("release")
  return "OrlixKernel" if name.match?(/kunit|kselftest|kernel/)

  "Build and release system"
end

def proof_map(target_map)
  candidates = target_map.fetch("targets").select do |target|
    target.fetch("phony") && target.fetch("name").match?(/test|proof|audit|gate|check|validate|prerequisite/)
  end
  {
    "schema" => 1,
    "purpose" => "Current proof command inventory and accepted product-runtime promotion order.",
    "promotion_order" => [
      { "tier" => 1, "name" => "Kernel dependency proof", "owner" => "OrlixKernel" },
      { "tier" => 2, "name" => "KUnit kernel-internal proof", "owner" => "OrlixKernel" },
      { "tier" => 3, "name" => "kselftest kernel-interface proof", "owner" => "OrlixKernel" },
      { "tier" => 4, "name" => "OrlixMLibC proof", "owner" => "OrlixMLibC" },
      { "tier" => 5, "name" => "OrlixMLibC-built syscall and UAPI proof", "owner" => "OrlixMLibC" },
      { "tier" => 6, "name" => "POSIX shell proof", "owner" => "OrlixOS" },
      { "tier" => 7, "name" => "jq proof", "owner" => "OrlixOS" },
      { "tier" => 8, "name" => "curl proof", "owner" => "OrlixOS" },
      { "tier" => 9, "name" => "zsh proof", "owner" => "OrlixOS" },
      { "tier" => 10, "name" => "Product integration proof", "owner" => "OrlixOS and Orlix" }
    ],
    "commands" => candidates.map do |target|
      target.merge("owner" => proof_owner(target), "artifact_digest_binding" => "not-yet-implemented")
    end.sort_by { |target| [target.fetch("source"), target.fetch("line"), target.fetch("name")] }
  }
end

def encoded(value)
  JSON.pretty_generate(value, indent: "  ", space: " ", object_nl: "\n", array_nl: "\n") + "\n"
end

target_map = legacy_target_map
outputs = {
  "legacy-target-map.json" => target_map,
  "xcode-target-map.json" => xcode_target_map,
  "workflow-map.json" => workflow_map,
  "proof-map.json" => proof_map(target_map)
}

mode = ARGV.fetch(0, "--check")
unless %w[--write --check].include?(mode)
  abort "usage: ruby bazel/migration/inventory.rb [--write|--check]"
end

failures = []
outputs.each do |name, value|
  path = OUTPUT_ROOT.join(name)
  content = encoded(value)
  if mode == "--write"
    path.write(content)
    puts "generated #{path.relative_path_from(ROOT)}"
  elsif !path.file? || path.read != content
    failures << path.relative_path_from(ROOT).to_s
  end
end

unless failures.empty?
  warn "migration inventory drift: #{failures.join(", ")}"
  exit 1
end

puts "Bazel migration inventory is current" if mode == "--check"
