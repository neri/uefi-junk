#
# Rakefile for EFI Hello world
#
require 'rake/clean'
require 'rake/packagetask'
# require 'json'
require 'yaml'
require 'base64'
require 'digest/sha2'

ARCH  = ENV['ARCH'] || case `uname -m`
when /i[3456]86/
  :i386
when /x86.64/
  :x64
when /aarch64/
  :aa64
else
  :unknown
end

PATH_BIN        = "bin/"
PATH_SRC        = "src/"
PATH_SRC_FONTS  = "#{PATH_SRC}fonts/include/"
PATH_OBJ        = "obj/"
PATH_MNT        = "mnt/"
PATH_VAR        = "var/"
PATH_EFI_BOOT   = "#{PATH_MNT}EFI/BOOT/"
PATH_INC        = "#{PATH_SRC}include/"
CP932_BIN       = "#{PATH_EFI_BOOT}cp932.bin"
PATH_EFI_VENDOR = "#{PATH_MNT}EFI/JUNK/"

case ARCH.to_sym
when :x64
  URL_OVMF      = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEX64_OVMF.fd"
  URL_SHELL     = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEX64_Shell.efi"
  QEMU_ARCH     = "x86_64"
  QEMU_OPTS     = "-smp 4 -rtc base=localtime -device nec-usb-xhci,id=xhci -device usb-kbd -device usb-mouse"
when :i386
  URL_OVMF      = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEIa32_OVMF.fd"
  URL_SHELL     = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEIA32_Shell.efi"
  QEMU_ARCH     = "x86_64"
  QEMU_OPTS     = "-smp 4 -rtc base=localtime -device nec-usb-xhci,id=xhci"
when :arm
  URL_OVMF      = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEARM_QEMU_EFI.fd"
  URL_SHELL     = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEARM_Shell.efi"
  QEMU_ARCH     = "aarch64"
  QEMU_OPTS     = "-M virt -cpu cortex-a15"
when :aa64
  URL_OVMF      = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEAARCH64_QEMU_EFI.fd"
  URL_SHELL     = "https://github.com/retrage/edk2-nightly/raw/master/bin/RELEASEAARCH64_Shell.efi"
  QEMU_ARCH     = "aarch64"
  QEMU_OPTS     = "-M virt -cpu cortex-a57"
else
  raise "UNKNOWN ARCH #{ARCH}"
end

if RUBY_PLATFORM =~ /darwin/ then
  LLVM_PREFIX     = `brew --prefix llvm`.gsub(/\n/, '')
  CC      = ENV['CC'] || "#{LLVM_PREFIX}/bin/clang"
  LD      = ENV['LD'] || "#{LLVM_PREFIX}/bin/lld-link"
else
  CC      = ENV['CC'] || "clang"
  LD      = ENV['LD'] || "lld-link-7.0"
end
CFLAGS  = "-Os -std=c11 -fno-stack-protector -fshort-wchar -mno-red-zone -nostdlibinc -I #{PATH_INC} -I #{PATH_SRC} -I #{PATH_SRC_FONTS} -Wall -Wpedantic -fno-exceptions"
AS      = ENV['AS'] || "nasm"
AFLAGS  = "-s -I #{ PATH_SRC }"
LFLAGS  = "-nodefaultlib -entry:efi_main"

INCS  = [FileList["#{PATH_SRC}*.h"], FileList["#{PATH_INC}*.h"]]

CLEAN.include(FileList["#{PATH_BIN}**/*"])
CLEAN.include(FileList["#{PATH_OBJ}**/*"])
CLEAN.include(CP932_BIN)

directory PATH_MNT
directory PATH_OBJ
directory PATH_BIN
directory PATH_EFI_BOOT
directory PATH_EFI_VENDOR
directory PATH_VAR

TASKS = [ :main ]

TASKS.each do |t|
  task t => [t.to_s + ":build"]
end

task :default => [PATH_OBJ, PATH_BIN, TASKS].flatten

def convert_arch(s)
  case s.to_sym
  when :x64
    ['x86_64-pc-win32-coff', 'x64']
  when :i386
    ['i386-pc-win32-coff', 'ia32']
  when :arm
    ['arm-pc-win32-coff', 'arm']
  when :aa64
    ['aarch64-pc-win32-coff', 'aa64']
  end
end

def make_curl(cputype, target, base_path, url)
  (cf_target, efi_suffix) = convert_arch(cputype)
  path_output = "#{base_path}#{target.gsub(/\{\:suffix\}/, efi_suffix)}"
  file path_output => [base_path] do |t|
    sh "curl -# -L -o #{t.name} #{url}"
  end
  path_output
end

PATH_SHELL = make_curl(ARCH, 'shell{:suffix}.efi', PATH_EFI_BOOT, URL_SHELL)
PATH_OVMF = make_curl(ARCH, 'ovmf{:suffix}.fd', PATH_VAR, URL_OVMF)

desc "Install to #{PATH_MNT}"
task :install => ["main:install".to_sym]

desc "Make an ISO image"
task :iso => [PATH_MNT, :install] do
  sh "mkisofs -r -J -o #{TARGET_ISO} #{PATH_MNT}"
end

desc "Run with QEMU"
task :run => [PATH_OVMF, :install] do
  sh "qemu-system-#{QEMU_ARCH} #{QEMU_OPTS} -bios #{PATH_OVMF} -monitor stdio -drive format=raw,file=fat:rw:mnt"
end


file CP932_BIN => [ PATH_EFI_BOOT, "#{PATH_SRC}cp932.txt"] do |t|
  bin = []
  File.open(t.prerequisites[1]) do |file|
    file.each_line do |line|
      bin << Base64.decode64(line)
    end
  end
  File.binwrite(t.name, bin.join(''))
  raise unless File.exist?(t.name)
end


def make_efi(cputype, target, src_tokens, options = {})

  (cf_target, efi_suffix) = convert_arch(cputype)

  case cputype.to_sym
  when :x64
    af_target = "-f win64"
  when :i386
    af_target = "-f win32"
  end

  path_obj      = "#{PATH_OBJ}#{efi_suffix}/"
  directory path_obj

  local_incs = []

  base_dir = options['base_dir'] || target
  path_src_p = "#{PATH_SRC}#{base_dir}/"
  local_incs << FileList["#{path_src_p}*.h"]

  if options['no_suffix']
    output = "#{PATH_BIN}#{target}"
  else
    output = "#{PATH_BIN}#{target}#{efi_suffix}.efi"
  end
  subsystem = options['subsystem'] || 'efi_application'

  rsrc = "#{path_src_p}rsrc.yml"
  if File.exist?(rsrc)
    rsrc_inc = "#{path_src_p}rsrc.h"
    INCS << rsrc_inc
    CLEAN.include(rsrc_inc)

    file rsrc_inc => [rsrc] do |t|
      rsrc_yaml = File.open(t.prerequisites[0]) do |file|
        YAML.load(file)
      end

      prefix = rsrc_yaml['prefix']

      File.open(t.name, 'w') do |file|
        file.puts '// AUTO GENERATED rsrc.h'

        enums = rsrc_yaml['enum']
        if enums
          enums.keys.each do |ns|
            file.puts "typedef enum {"
            strings = enums[ns].map do |item|
              result_string = item
              if item.is_a?(Hash)
                key = item.keys[0]
                result_string = key
                file.puts "\t#{ns}_#{key} = #{item[key]},"
              else
                file.puts "\t#{ns}_#{item.gsub(/\W/, '_')},"
              end
              result_string
            end
            file.puts "\t#{ns}_max"
            file.puts "} #{ns}_enum;"
            file.puts "const char * #{ns}_strings[] = {"
            strings.each do |s|
              file.puts "\t#{s.inspect},"
            end
            file.puts "};"
          end
        end

        strings = rsrc_yaml['strings']
        if strings
          keys = strings['default'].keys
          file.puts "typedef enum {"
          keys.each do |key|
            file.puts "\t#{prefix}_#{key},"
          end
          file.puts "\t#{prefix}_max"
          file.puts "} #{prefix};"

          %w(default ja).each do |lang|
            if strings[lang]
              file.puts "const char* #{prefix}_#{lang}[] = {"
              keys.each do |key|
                value = strings[lang][key]
                file.puts "\t#{value ? value.inspect : 'NULL'},"
              end
              file.puts "};"
            end
          end
        end

        file.puts "\n"
      end
    end
  end

  srcs = {}
  src_tokens.each do |s|
    t = nil
    if s !~ /\.\w+/
      s += '.c'
    end
    base = File.basename(s, '.*')
    ext = File.extname(s)
    if s !~ /\//
      t = [
        "#{path_src_p}#{s}",
        "#{path_src_p}#{base}-#{efi_suffix}#{ext}",
        "#{PATH_SRC}#{s}",
        "#{PATH_SRC}#{base}-#{efi_suffix}#{ext}",
      ].find do |p|
        if File.exist?(p)
          p
        end
      end
    end
    srcs[s] = t || s
  end

  objs = srcs.map do |token, src|
    mod_name = File.basename(token, '.*')
    hash = Digest::SHA256.hexdigest("#{File.dirname(src)}:#{options['cflags']}").slice(0, 16)
    obj = "#{path_obj}#{mod_name}-#{hash}.o"

    case File.extname(src)
    when '.c'
      file obj => [ src, INCS, local_incs, path_obj ].flatten do |t|
        sh "#{ CC } -target #{ cf_target } #{ CFLAGS } #{ options['cflags'] } -c -o #{ t.name } #{ src }"
      end
    when '.asm'
      file obj => [ src, path_obj ] do | t |
        sh "#{ AS } #{ af_target } #{ AFLAGS } -o #{ t.name } #{ src }"
      end
    end

    obj
  end

  file output => [PATH_BIN, objs].flatten do |t|
    sh "#{LD} -subsystem:#{subsystem} #{ LFLAGS} #{ objs.join(' ') } -out:#{ t.name }"
  end

  output
end


namespace :main do

  targets = {}

  build_yml = File.open("build.yml") do |file|
    YAML.load(file)
  end
  target_config = build_yml['targets']

  target_config.keys.each do |target|
    sources = target_config[target]['sources']
    allow = target_config[target]['valid_arch'] || 'all'
    if allow == 'all' || allow.include?(ARCH.to_s) then
      targets[target] = make_efi(ARCH, target, sources, target_config[target])
    end
  end

  desc "Build Main"
  task :build => targets.values

  install_targets = []
  (_, efi_suffix) = convert_arch(ARCH)
  target_config.keys.each do |t|
    config = target_config[t]
    bin = targets[t]
    if bin
      move_to = config['efi_bootloader'] ? "#{PATH_EFI_BOOT}boot#{efi_suffix}.efi" : "#{PATH_EFI_VENDOR}#{File.basename(bin)}"
      file move_to => bin do
        puts "copy #{bin.inspect} to #{move_to.inspect}"
        FileUtils.cp bin, move_to
      end
      install_targets << move_to
    end
  end

  task :install => [:build, PATH_VAR, PATH_EFI_BOOT, PATH_EFI_VENDOR, PATH_OVMF, CP932_BIN, PATH_SHELL, install_targets].flatten

end
