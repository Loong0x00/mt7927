#!/usr/bin/env ruby

require 'optparse'
require 'fileutils'

def assert_warn(warning)
  yield or warn "* warning: #{warning}"
end

def assert_error(warning)
  yield or begin
    warn "* error: #{warning}"
    exit
  end
end

verbose = false
output = nil

OptionParser.new do |opts|
  opts.banner = "Usage: #{$0} [options] FILE"

  opts.on("-v", "--[no-]verbose", "Run verbosely") { |v| verbose = v }
  opts.on("-o", "--output=DIRECTORY", "Output directory") { |o| output = o }
end.parse!

assert_error("output directory not given (run with --help)") { output }

file = ARGF.read.b

iter = 0

magic, nitems, unk1, fsz, fzero = file.unpack("A4SSII")
assert_error("magic must be 'MTK-'") { magic == "MTK-" }
assert_warn("unk1 should be 1?") { unk1 == 1 }
assert_warn("fzero should be zero?") { fzero == 0 }
assert_warn("fsz should be equal to file size") { fsz == file.length }
iter += 16

files = {}

nitems.times do |i|
  fn, date, ioff, isz, izero = file.unpack("A48A16III", offset: iter)
  assert_warn("#{fn}: izero should be zero?") { izero == 0 }
  iter += 48 + 16 + 4 + 4 + 4
  files[fn] = {date:, ioff:, isz:, izero:}
end

pp({magic:, nitems:, unk1:, fsz:, fzero:, files:}) if verbose

FileUtils.mkdir_p(output)

files.each do |k,v|
  File.binwrite("#{output}/#{k}", file[v[:ioff], v[:isz]])
end
