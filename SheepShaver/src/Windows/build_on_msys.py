#
# A python 2.7 script to fetch all dependencies and build Sheepshaver using MSYS / mingw
#
import argparse
import glob
import json
import os
import subprocess
import urllib
import xml.dom.minidom
import zipfile
import sys
from contextlib import contextmanager
import datetime
import shutil

# TODO keep track of the values for these flags used for the previous compile and
# redo the macemu configures if they change

MACEMU_CFLAGS = "-mwin32"
MACEMU_CXXFLAGS = "-mwin32 -std=gnu++11"

script_path = os.path.dirname(os.path.abspath(__file__))


MINGW_EXTRACT_PATH = r"c:\mingw-sheep"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--uninstall-packages", "-u",
                        help="Uninstall all the msys/mingw packages",
                        default=False,
                        action="store_true",
                        )
    parser.add_argument("--run-shell", "-s",
                        help="Run a quick bash session with the paths set",
                        default=False,
                        action="store_true",
                        )
    parser.add_argument("--make-threads", "-j",
                        default=1,
                        type=int,
                        )
    parser.add_argument("--gitignore-link-outputs",
                        help="Add the symlinks that 'make links' creates to .gitignore and stop tracking them",
                        default=False,
                        action="store_true",
                        )
    parser.add_argument("--show-build-environment",
                        default=False,
                        action="store_true",
                        )
    parser.add_argument("--install-to-dir",
                        default=None,
                        help="Copy the resulting exe to the given directory after building")
    parser.add_argument("--run-shell-command", "-c",
                        help="Run a command in the mingw shell")
    parser.add_argument("--use-precompiled-dyngen",
                        default=False,
                        action="store_true")
    parser.add_argument("--build-jit",
                        default=False,
                        action="store_true")
    parser.add_argument("--debug-build",
                        default=False,
                        action="store_true",
                        help="disable things in the build that are a problem for debugging")
    parser.add_argument("--add-path",
                        default=None,
                        help="Add something to the PATH used for builds, with highest priority")
    parser.add_argument("--build",
                        default=None,
                        help="Build platform to pass to configure scripts")
    parser.add_argument("--alt-mingw-path",
                        dest="alt_mingw_path",
                        default=None,
                        help="Path to use for mingw install")
    return parser.parse_args()


def get_download_dir():
    download_dir = os.path.join(script_path, "downloads")
    if not os.path.isdir(download_dir):
        os.mkdir(download_dir)
    return download_dir


def download(url, local_filename_proper=None):
    if local_filename_proper is None:
        up_to_path = url.rsplit("?", 1)[0]
        local_filename_proper = up_to_path.rsplit("/", 1)[-1]
    local_filename = os.path.join(get_download_dir(), local_filename_proper)
    if not os.path.exists(local_filename):
        try:
            urllib.urlretrieve(url, local_filename)
        except IOError:
            if os.path.exists(local_filename):
                os.remove(local_filename)
            raise
    return local_filename


def extract_zip(zip_filename, target_dir):
    if not os.path.isdir(target_dir):
        os.mkdir(target_dir)
    zf = zipfile.ZipFile(zip_filename)
    try:
        zf.extractall(target_dir)
    finally:
        zf.close()


def env_augmented_with_paths(*path_dirs_to_add):
    env_copy = dict(os.environ)
    path_dirs = env_copy["PATH"].split(os.pathsep)
    for d in path_dirs_to_add:
        if d not in path_dirs:
            path_dirs.append(d)
    env_copy["PATH"] = os.pathsep.join(path_dirs)
    return env_copy


def display_dir(path):
    windows_dir = os.path.join(script_path)
    if path.startswith(windows_dir):
        return "Windows" + path[len(windows_dir):]
    return path


def run(cmd_args, *args, **kwargs):
    print "%s (cwd=%s)" % (" ".join(cmd_args), kwargs.get("cwd"))
    sys.stdout.flush()
    subprocess.check_call(cmd_args, *args, **kwargs)


def log(msg):
    print msg
    sys.stdout.flush()


def install(make_args, show_build_environment, use_precompiled_dyngen, build_jit, debug_build,
            install_to_dir=None, add_path=None, build=None):

    root_dir = os.path.abspath(os.path.join(script_path, "..", "..", ".."))
    dep_tracker = BuildDepTracker(root_dir)

    # just ham in the working directory for consistency
    os.chdir(script_path)

    # get msys / mingw tools we need to build

    mingw_get_zip = download("https://downloads.sourceforge.net/project/mingw/Installer/mingw-get/"
                             "mingw-get-0.6.2-beta-20131004-1/mingw-get-0.6.2-mingw32-beta-20131004-1-bin.zip")
    mingw_get_dir = MINGW_EXTRACT_PATH
    extract_zip(mingw_get_zip, mingw_get_dir)

    mingw_get_filename = os.path.join(mingw_get_dir, "bin", "mingw-get.exe")
    assert os.path.isfile(mingw_get_filename)

    msys_packages = ["bash", "autogen", "m4", "make", "patch", "grep", "sed"]
    mingw_packages = ["autoconf", "autoconf2.5", "automake", "automake1.11", "binutils", "base", "autotools", "libtool",
                      "gcc", "gcc-g++"]

    all_packages_to_install = ["mingw32-%s" % x for x in mingw_packages] + ["msys-%s" % x for x in msys_packages]

    installed_packages = get_installed_packages(quiet=True)

    if any(package not in installed_packages for package in all_packages_to_install):
        run([mingw_get_filename, "install"] + all_packages_to_install)
    else:
        log("All required packages installed")

    mingw_get_bin_dir = os.path.join(mingw_get_dir, "bin")
    msys_bin_dir = os.path.join(mingw_get_dir, "msys", "1.0", "bin")
    mingw_bin_dir = os.path.join(mingw_get_dir, "mingw32", "bin")
    
    paths_to_add = [mingw_get_bin_dir, msys_bin_dir, mingw_bin_dir]
    if add_path is not None:
        paths_to_add.insert(0, add_path)
    our_env = env_augmented_with_paths(*paths_to_add)

    # ditch any outer make flags from cmake or similar due to compatibility problems
    for var in ["MAKEFLAGS", "MAKELEVEL", "MFLAGS"]:
        our_env.pop(var, None)
        
    if show_build_environment:
        print "ENVIRONMENT FOR BUILD"
        show_env_dict(our_env)
        print ""

    # build SDL

    sdl_zip_filename = download("http://www.libsdl.org/release/SDL-1.2.15.zip")
    sdl_dir = os.path.join(get_download_dir(), "SDL-1.2.15")
    with dep_tracker.rebuilding_if_needed("sdl_extract_zip", sdl_zip_filename) as needs_rebuild:
        if needs_rebuild:
            extract_zip(sdl_zip_filename, get_download_dir())

    patch_filename = os.path.join(script_path, "sdl_fix.patch")
    sdl_patched_files = ["build-scripts/ltmain.sh"]
    with dep_tracker.rebuilding_if_needed("sdl_patch", patch_filename) as needs_rebuild:
        if needs_rebuild:
            # apply patch for building in msys/mingw
            patch_exe = os.path.join(msys_bin_dir, "patch.exe")
            run([patch_exe, "-p2", "-i", patch_filename], cwd=sdl_dir, env=our_env)
            dep_tracker.done("sdl_patch")

    msys_bash = os.path.join(msys_bin_dir, "bash.exe")

    make_bin = os.path.join(msys_bin_dir, "make.exe")
    # our_env["MAKE"] = make_bin

    with dep_tracker.rebuilding_if_needed("sdl_autogen",
                                          ["configure.in"] + sdl_patched_files, base_dir=sdl_dir) as needs_rebuild:
        if needs_rebuild:
            run([msys_bash, "./autogen.sh"], cwd=sdl_dir, env=our_env)
    with dep_tracker.rebuilding_if_needed("sdl_configure", "configure", base_dir=sdl_dir) as needs_rebuild:
        if needs_rebuild:
            sdl_configure_args = [msys_bash, "./configure", "--disable-shared", "--prefix=/usr"]
            if build is not None:
                sdl_configure_args += ["--build", build]
            run(sdl_configure_args, cwd=sdl_dir, env=our_env)
            run([make_bin] + make_args + ["clean"], cwd=sdl_dir, env=our_env)

    run([make_bin] + make_args, cwd=sdl_dir, env=our_env)

    # TODO track all the files that this could install
    sdl_headers = "SDL.h SDL_active.h SDL_audio.h SDL_byteorder.h SDL_cdrom.h SDL_cpuinfo.h SDL_endian.h " \
                  "SDL_error.h SDL_events.h SDL_getenv.h SDL_joystick.h SDL_keyboard.h SDL_keysym.h SDL_loadso.h " \
                  "SDL_main.h SDL_mouse.h SDL_mutex.h SDL_name.h SDL_opengl.h SDL_platform.h SDL_quit.h SDL_rwops.h " \
                  "SDL_stdinc.h SDL_syswm.h SDL_thread.h SDL_timer.h SDL_types.h SDL_version.h SDL_video.h " \
                  "begin_code.h close_code.h"
    sdl_headers = ["include/" + x for x in sdl_headers.split(" ")]
    sdl_files_being_installed = ["sdl-config", "build/libSDL.la"] + sdl_headers

    with dep_tracker.rebuilding_if_needed("sdl_install", sdl_files_being_installed, base_dir=sdl_dir) as needs_rebuild:
        if needs_rebuild:
            run([make_bin, "install"], cwd=sdl_dir, env=our_env)

    # build sheepshaver

    sheepshaver_dir = os.path.abspath(os.path.join(script_path, "..", ".."))
    print "SHEEPSHAVER_DIR: %s" % sheepshaver_dir

    link_input_prefix = "BasiliskII/src/"
    link_output_prefix = "SheepShaver/src/"
    link_inputs = get_symlink_filenames(prefix=link_input_prefix)

    print "Tracking %d link inputs" % len(link_inputs)
    sys.stdout.flush()

    stale_or_missing_dir_contents_files = False
    for link_input_proper in link_inputs:
        link_input = os.path.join(root_dir, link_input_proper)
        if os.path.isdir(link_input):
            assert link_input_proper.startswith(link_input_prefix)
            link_output_proper = link_output_prefix + link_input_proper[len(link_input_prefix):]
            link_input_mtime = dep_tracker.get_inputs_modified_time([link_input])
            link_output = os.path.join(root_dir, link_output_proper)
            if os.path.isdir(link_output):
                link_output_mtime = dep_tracker.get_inputs_modified_time([link_output])
                if link_output_mtime is None or link_output_mtime < link_input_mtime:
                    shutil.rmtree(link_output)
                    stale_or_missing_dir_contents_files = True
            else:
                stale_or_missing_dir_contents_files = True

    # TODO: fix make links step rather than just eating the exception
    with dep_tracker.rebuilding_if_needed("sheepshaver_top_makefile", ["SheepShaver/Makefile"] + link_inputs,
                                          base_dir=root_dir) as needs_rebuild:
        if needs_rebuild or stale_or_missing_dir_contents_files:
            try:
                run([make_bin, "links"], cwd=sheepshaver_dir, env=our_env)
            except subprocess.CalledProcessError:
                pass

    autogen_env = dict(our_env, NO_CONFIGURE="1")

    unix_dir = os.path.join(sheepshaver_dir, "src", "Unix")

    with dep_tracker.rebuilding_if_needed("sheepshaver_autogen", ["configure.ac"],
                                          base_dir=script_path) as needs_rebuild:
        if needs_rebuild:
            run([msys_bash, os.path.join(unix_dir, "autogen.sh")], cwd=script_path, env=autogen_env)

    ln_cmd = os.path.join(msys_bin_dir, "ln.exe")

    windows_m4_dir = os.path.join(script_path, "m4")
    if not os.path.exists(windows_m4_dir):
        run([ln_cmd, "-sf", os.path.join(unix_dir, "m4"), windows_m4_dir],
            cwd=script_path, env=autogen_env)

    configure_macemu_env = dict(our_env)
    configure_macemu_env["CC"] = "gcc %s" % MACEMU_CFLAGS
    configure_macemu_env["CXX"] = "g++ %s" % MACEMU_CXXFLAGS

    did_configure = False
    with dep_tracker.rebuilding_if_needed("sheepshaver_configure", ["configure", "Makefile.in"],
                                          base_dir=script_path) as needs_rebuild:
        if needs_rebuild:
            config_status_filename = os.path.join(script_path, "config.status")
            if os.path.exists(config_status_filename):
                log("removing existing config.status")
                os.remove(config_status_filename)
            sheepshaver_configure_options = []
            if not build_jit:
                sheepshaver_configure_options.append("--enable-jit=no")
            if debug_build:
                sheepshaver_configure_options.append("--enable-vosf=no")
            run([msys_bash, "./configure", "--with-gtk=no"] + sheepshaver_configure_options,
                cwd=script_path, env=configure_macemu_env)
            did_configure = True

    if did_configure:
        run([make_bin, "clean"], cwd=script_path, env=our_env)

    sheepshaver_make_args = list(make_args)
            
    if use_precompiled_dyngen and build_jit:
        for precompiled_dyngen_file, target_dir, target_dyngen_file in (
            ("dyngen_precompiled/ppc-execute-impl.cpp", ".", "ppc-execute-impl.cpp"),
            ("dyngen_precompiled/basic-dyngen-ops-x86_32.hpp", "../Unix", "basic-dyngen-ops.hpp"),
            ("dyngen_precompiled/ppc-dyngen-ops-x86_32.hpp", "../kpx_cpu", "ppc-dyngen-ops.hpp"),
        ):
            log("Copying %s to %s" % (precompiled_dyngen_file, target_dyngen_file))
            shutil.copy(os.path.join(script_path, "..", "Unix", precompiled_dyngen_file),
                        os.path.join(script_path, target_dyngen_file)
                        )
        sheepshaver_make_args.append("USE_DYNGEN=no")

    run([make_bin] + sheepshaver_make_args, cwd=script_path, env=our_env)

    if install_to_dir is not None:
        assert os.path.isdir(install_to_dir)
        binary_name = "SheepShaver.exe"
        dest_filename = os.path.join(install_to_dir, binary_name)
        log("Creating %s" % dest_filename)
        shutil.copy(os.path.join(script_path, binary_name), dest_filename)


def show_env_dict(d):
    keys = d.keys()
    keys.sort()
    for key in keys:
        value = d[key]
        print "\t%20s\t%s" % (key, value)


def xml_element_helper(filename, tag_name):
    dom = xml.dom.minidom.parse(filename)
    for element in dom.getElementsByTagName(tag_name):
        assert isinstance(element, xml.dom.minidom.Element)
        yield element


def xml_read_helper(filename, tag_name, attribute):
    """In the named XML file, get the values for an attribute on all the tags with the given tag name,
    and return them as a list"""
    values = []
    for element in xml_element_helper(filename, tag_name):
        value = element.getAttribute(attribute)
        values.append(value)
    return values


def get_installed_packages(quiet=False):
    data_path = os.path.join(MINGW_EXTRACT_PATH, "var", "lib", "mingw-get", "data")

    # msys tracks installed packages by installed tarnames in the sysroot file.

    # first, get the mapping from tarnames to package names for all available packages
    package_list_filename = os.path.join(data_path, "package-list.xml")
    if not os.path.exists(package_list_filename):
        return []
    catalogues = xml_read_helper(package_list_filename, "package-list", "catalogue")

    packages_by_tarname = {}

    for catalogue in catalogues:
        catalog_filename = os.path.join(data_path, catalogue + ".xml")
        package_catalogues = xml_read_helper(catalog_filename, "package-list", "catalogue")

        for package_catalogue in package_catalogues:
            package_catalogue_filename = os.path.join(data_path, package_catalogue + ".xml")
            for package_element in xml_element_helper(package_catalogue_filename, "package"):
                assert isinstance(package_element, xml.dom.minidom.Element)
                package_name = package_element.getAttribute("name")
                releases = package_element.getElementsByTagName("release")
                for release in releases:
                    assert isinstance(release, xml.dom.minidom.Element)
                    tarname = release.getAttribute("tarname")
                    skip_set = False
                    if tarname in packages_by_tarname:
                        old_package_name = packages_by_tarname[tarname]
                        if old_package_name != package_name:
                            if package_name.endswith("-old"):
                                skip_set = True
                            else:
                                assert False, "duplicate packages for %r; old: %r, " \
                                              "new %r" % (tarname, packages_by_tarname[tarname], package_name)
                    if not skip_set:
                        packages_by_tarname[tarname] = package_name

    # for tarname, package_name in packages_by_tarname.iteritems():
    #     print "%s -> %s" % (tarname, package_name)

    # next, get the list of all the installed tarnames, and build the list of installed packages for them
    installed_packages = set()
    for match_proper in glob.glob1(data_path, "sysroot-*.xml"):
        match_filename = os.path.join(data_path, match_proper)

        for tarname in xml_read_helper(match_filename, "installed", "tarname"):
            package_catalogue = packages_by_tarname[tarname]
            if not quiet:
                print "%s - %s" % (tarname, package_catalogue)
                sys.stdout.flush()
            installed_packages.add(package_catalogue)

    return list(installed_packages)


def uninstall_packages():
    mingw_get_filename = os.path.join(MINGW_EXTRACT_PATH, "bin", "mingw-get.exe")
    # output = subprocess.check_output([mingw_get_filename, "list"])

    installed_packages = get_installed_packages()

    # uninstall them
    for package_name in installed_packages:
        run([mingw_get_filename, "remove", package_name])


def run_shell(command=None):
    mingw_get_dir = MINGW_EXTRACT_PATH

    msys_bin_dir = os.path.join(mingw_get_dir, "msys", "1.0", "bin")
    mingw_bin_dir = os.path.join(mingw_get_dir, "mingw32", "bin")
    mingw_get_bin_dir = os.path.join(mingw_get_dir, "bin")

    msys_bash = os.path.join(msys_bin_dir, "bash.exe")

    our_env = env_augmented_with_paths(mingw_get_bin_dir, msys_bin_dir, mingw_bin_dir)

    args = [msys_bash]
    if command is not None:
        args += ["-c", command]
    run(args, env=our_env)


def get_symlink_filenames(prefix="SheepShaver/src/"):
    """ Get a list of files that 'make links' in the top Makefile makes symlinks for """
    sheepshaver_dir = os.path.abspath(os.path.join(script_path, "..", ".."))
    top_makefile = os.path.join(sheepshaver_dir, "Makefile")

    with open(top_makefile, "r") as handle:
        while not handle.readline().startswith("links:"):
            pass
        links_list_prefix = "	@list='"
        while True:
            first_line = handle.readline()
            if first_line.startswith(links_list_prefix):
                break
        assert first_line.startswith(links_list_prefix)
        lines = [first_line[len(links_list_prefix):]]
        while True:
            line = handle.readline()
            end_pos = line.find("'")
            if end_pos == -1:
                lines.append(line)
            else:
                lines.append(line[:end_pos])
                break

    links_list_text = "".join(lines)
    return [prefix + x for x in links_list_text.split() if x != "\\"]


def gitignore_patterns(patterns):
    """ Add the given patterns to the .gitignore file so they don't show up in git diff output """

    root_dir = os.path.abspath(os.path.join(script_path, "..", "..", ".."))
    gitignore_file = os.path.join(root_dir, ".gitignore")

    with open(gitignore_file, "a") as handle:
        for pattern in patterns:
            print >> handle, pattern


def get_tracked_files():
    root_dir = os.path.abspath(os.path.join(script_path, "..", "..", ".."))
    lines = subprocess.check_output(["git", "ls-tree", "--full-tree", "-r",
                                     "--name-only", "HEAD"], cwd=root_dir).split("\n")
    lines = [line.strip() for line in lines]
    return [line for line in lines if line != ""]


def get_staged_deletes():
    staged_deletes = []
    root_dir = os.path.abspath(os.path.join(script_path, "..", "..", ".."))
    lines = subprocess.check_output(["git", "diff", "--name-status", "--cached"], cwd=root_dir).split("\n")
    for line in lines:
        if line.strip() == "":
            continue
        flags, filename = line.split(None, 1)
        # print repr((flags, filename))
        if flags == "D":
            staged_deletes.append(filename)
    return staged_deletes


class BuildDepTracker(object):
    def __init__(self, root_path):
        assert os.path.isdir(root_path)
        self.root_path = root_path
        self.filename = os.path.join(script_path, "build_on_msys.cache.json")
        self.cache = None
        self.step_input_files = {}
        self.load()
        self.debug_output = True

    def load(self):
        if os.path.exists(self.filename):
            with open(self.filename, "r") as handle:
                try:
                    self.cache = json.load(handle)
                except ValueError:
                    msg = "ERROR: There was a problem loading the JSON cache file %s. " \
                          "Maybe check the file for a syntax error?" % self.filename
                    print >> sys.stderr, msg
                    sys.stderr.flush()
                    raise Exception(msg)
        else:
            self.cache = {"steps": {}}

    def save(self):
        with open(self.filename, "w") as handle:
            json.dump(self.cache, handle, sort_keys=True, indent=4, separators=(',', ': '))

    def _rel_path_for(self, filename):
        assert os.path.commonprefix(filename, self.root_path) == self.root_path
        return os.path.relpath(filename, start=self.root_path)

    @contextmanager
    def rebuilding_if_needed(self, step_name, input_filenames, base_dir=None):
        """
        @type step_name: unicode or str
        @type input_filenames: str or list of str
        @param base_dir: if provided, all input_filenames are relative to this"""
        needs_rebuild = self.check_needs_rebuild(step_name, input_filenames, base_dir)
        yield needs_rebuild
        if needs_rebuild:
            self.done(step_name)

    def check_needs_rebuild(self, step_name, input_filenames, base_dir=None):
        """ Check if the given step build name needs a rebuild based on our records and
        the ages of the input files.
        @type step_name: unicode or str
        @type input_filenames: list of str
        @param base_dir: if provided, all input_filenames are relative to this
        """
        if type(input_filenames) in (unicode, str):
            input_filenames = [input_filenames]
        if base_dir is not None:
            input_filenames = [os.path.join(base_dir, x) for x in input_filenames]
        step_entries = self.cache["steps"]
        if len(input_filenames) == 0:
            assert False, "At least one input file is required in step '%s'" % step_name

        entry = step_entries.get(step_name)

        input_modified_time = self.get_inputs_modified_time(input_filenames)

        self.step_input_files[step_name] = input_filenames

        rebuild_required = entry is None or entry < input_modified_time

        if self.debug_output:
            if entry is None:
                desc = "not previously built; building"
            else:
                desc = "rebuild required: %s; last build %s;" \
                       "input files changed %s; " % (rebuild_required,
                                                     datetime.datetime.fromtimestamp(entry),
                                                     datetime.datetime.fromtimestamp(input_modified_time),
                                                     )
            print "REBUILD(%s): %s" % (step_name, desc)
            sys.stdout.flush()

        return rebuild_required

    @classmethod
    def get_inputs_modified_time(cls, input_filenames):
        input_file_mtimes = []
        for input_filename in input_filenames:
            if os.path.isdir(input_filename):
                subtime = cls.get_inputs_modified_time(os.path.join(input_filename, s) for s in os.listdir(input_filename))
                if subtime is not None:
                    input_file_mtimes.append(subtime)
            stat = os.stat(input_filename)
            if stat is None:
                assert False, "Missing input file %s" % input_filename
            input_file_mtimes.append(stat.st_mtime)
        if not input_file_mtimes:
            return None
        input_modified_time = max(input_file_mtimes)
        return input_modified_time

    def done(self, step_name):
        print "DONE_REBUILD(%s)" % step_name
        input_filenames = self.step_input_files.get(step_name)
        if input_filenames is None:
            assert False, "No needs_rebuild check was done for step '%s' so we don't know its input files" % step_name

        step_entries = self.cache["steps"]
        input_modified_time = self.get_inputs_modified_time(input_filenames)

        # save a new entry for the build that happened
        # build is good up for the current file modified times
        step_entries[step_name] = input_modified_time
        self.save()


def main():
    global MINGW_EXTRACT_PATH
    options = parse_args()

    if options.alt_mingw_path:
        MINGW_EXTRACT_PATH = options.alt_mingw_path

    if options.run_shell:
        run_shell()
    elif options.run_shell_command is not None:
        run_shell(options.run_shell_command)
    elif options.uninstall_packages:
        uninstall_packages()
    elif options.gitignore_link_outputs:
        link_output_files = get_symlink_filenames()
        # add to gitignore
        gitignore_patterns(link_output_files)

        # stop tracking if tracked
        tracked_files = get_tracked_files()

        staged_deletes = get_staged_deletes()

        root_dir = os.path.abspath(os.path.join(script_path, "..", "..", ".."))
        for filename_relative in link_output_files:
            # filename = os.path.join(root_dir, filename_relative.replace("/", "\\"))
            file_is_tracked = filename_relative in tracked_files or \
                              any(filename.startswith(filename_relative + "/") for filename in tracked_files)
            if file_is_tracked and filename_relative not in staged_deletes:
                subprocess.check_call(["git", "rm", "--cached", filename_relative], cwd=root_dir)

    else:
        make_args = []
        num_threads = options.make_threads
        if num_threads > 1:
            make_args.append("-j%d" % num_threads)

        if options.install_to_dir is not None:
            log("Will install to %s" % options.install_to_dir)
        install(make_args, options.show_build_environment, options.use_precompiled_dyngen, options.build_jit,
                options.debug_build, install_to_dir=options.install_to_dir, add_path=options.add_path,
                build=options.build)


if __name__ == "__main__":
    main()
