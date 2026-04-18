#!/usr/bin/env python
#
# Cg compiler server.
#
# The server expects connections on port 4455 and accepts Cg programs.  The Cg
# programs are compiled using the Cell SDK Cg compiler (sce-cgc). The resulting
# output file is sent back to the connecting client.
#
# Protocol:
# A request has the following format:
#	- Length of the parameter string (big endian, 32bit).
#	- Length of the comment string (big endian, 32bit).
#	- Length of the program text (big endian, 32bit).
#	- The parameter string (see below), no null-termination.  The parameter
#	  string may start with the seqeunce '@NAME:', where NAME is the name
#	  of the Cg compiler to be used.  NAME is one of the following:
#	  * sce
#	    The Cg compiler provided by SONY as part of the Cell SDK.
#	  If the compiler name prefix is omitted, then the SONY compiler is
#	  used.
#	- The comment string.  This is an opaque comment string created by the
#	  client.  May contain a reference to the CFX file and/or parameters
#	  used when creating the program text.  No null-termination.
#	- The program text, no null-termination.
# The parameter string is a comma separated string of compiler parameters.
# Parameters starting with a hyphen are passed to the compiler as specified
# (eg. '-profile sce_vp_TypeC'), parameters not starting with a hyphen are
# passed to the compiler using the '-D' option.  The resulting output file is
# sent back to the client.  In case of a compilation error, the server will
# terminate without sending a response to the client.

import sys, os, socket, struct, re, time, getopt
import traceback

VERSION = '0.2'

basedir = None
log_cg = False
log_bin = False
extra_options = [ ]
verbose = 0
do_preprocess = True
do_wrap = True
do_remap = True
do_simplify = False

log_filename = 'cgserver.log'

srv_port = 4455
cell_sdk = None
cg_compiler = { }
if os.uname()[0] == 'Linux':
  host_system = 'linux'
else:
  host_system = 'win32'
if host_system == 'linux':
  tmp_dir = '/tmp'
else:
  tmp_dir = 'e:/tmp'
tmp_counter = 0

def version(out):
  out.write('Cg Server Version ' + str(VERSION) + '\n')
  out.flush()

def help(out):
  out.write(
"""Command options:
-h|--help
  Display this help screen and exit.
-V|--version
  Display the program version information and exit.
-v|--verbose
  Verbose output.  (Not implemented.)
-s|--silent
  Silent operation.  (Not implemented.)
-p|--port PORT
  The Cg server port number.  The default value is 4455.
-d|--dir DIR
  The output base directory for debug/logging information.
--log-cg
  Log all incoming Cg programs to the output directory.
--log-bin
  Log all generated compiled binary files to the output directory.
--nowrap
  Don't wrap the generated compiled binary files (compatibility option).
--noremap
  Don't remap unsupported input semantics for vertex programs (will cause a
  compile error if the program contains unsupported semantics).
--simplify
  If a program fails to compile, try to simplify the program code.
-x|--extra OPTION
  Add extra options to the Cg compiler command line.
""")
  out.flush()

def log(message, prefix = '', exception = None):
  global log_filename, basedir

  sys.stdout.write('cgserver: ' + prefix + str(message) + '\n')
  f = None
  if log_filename is not None:
    if basedir is not None:
      filename = basedir + '/' + log_filename
    else:
      filename = log_filename
    f = file(filename, 'a')
    f.write(prefix + str(message) + '\n')
  if exception is not None:
    traceback.print_exc(file = f)
    traceback.print_exc(file = sys.stdout)
  if f is  not None: f.close()

def info(message, exception = None):
  log(message, '', exception)

def warning(message, exception = None):
  log(message, 'WARNING: ', exception)

def error(message, exception = None):
  log(message, 'ERROR: ', exception)

def debug(message, exception = None):
  log(message, 'DEBUG: ', exception)

def init():
  global cell_sdk, cg_compiler, host_system, verbose

  info("""
_____________________________________________________________________________
LOG STARTED

""")
  cell_sdk = os.getenv('CELL_SDK')
  cell_sdk_version = os.getenv('CELL_SDK_VERSION')
  if cell_sdk_version is not None: cell_sdk += '/' + cell_sdk_version
  if host_system == 'win32': cell_sdk = cell_sdk.replace('\\', '/')
  if host_system == 'win32': cell_sdk = cell_sdk.replace('/e/', 'e:/')  
  cg_compiler['sce'] = cell_sdk + '/host-' + host_system + '/Cg/bin/sce-cgc'
  if not os.access(cg_compiler['sce'], os.X_OK):
    error('Can not find Cg compiler ' + repr(cg_compiler['sce']))
    sys.exit(1)
  if verbose > 0:
    for name, compiler in cg_compiler.items():
      info('Using Cg compiler [' + name + ']: ' + repr(compiler))

def tmpnam():
  global tmp_dir, tmp_counter

  tmp_counter += 1
  return (tmp_dir + '/cgserver_' + str(os.getpid())
      + '_' + str(tmp_counter))

class PreProcess:

  __vp_semantics = [
    [ 'ATTR0', 'POSITION0' ],
    [ 'ATTR1', 'BLENDWEIGHT0' ],
    [ 'ATTR2', 'NORMAL0' ],
    [ 'ATTR3', 'COLOR0', 'DIFFUSE0' ],
    [ 'ATTR4', 'COLOR1', 'SPECULAR0' ],
    [ 'ATTR5', 'FOGCOORD0', 'TESSFACTOR0' ],
    [ 'ATTR6', 'PSIZE0' ],
    [ 'ATTR7', 'BLENDINDICES0' ],
    [ 'ATTR8', 'TEXCOORD0' ],
    [ 'ATTR9', 'TEXCOORD1' ],
    [ 'ATTR10', 'TEXCOORD2' ],
    [ 'ATTR11', 'TEXCOORD3' ],
    [ 'ATTR12', 'TEXCOORD4' ],
    [ 'ATTR13', 'TEXCOORD5' ],
    [ 'ATTR14', 'TEXCOORD6', 'TANGENT0' ],
    [ 'ATTR15', 'TEXCOORD7', 'BINORMAL0' ]
  ]

  def __init__(self, program, entry, profile):
    """Construct for the program preprocessor.

    :Parameters:
      - `program`: The Cg program.
      - `entry`: The name of the entry function.
      - `profile`: The selected profile.
    """

    self.__input_program = program
    self.__program = self.__strip_comments(program)
    self.__profile = profile
    self.__entry = entry
    self.__input_decl = None
    self.__input_struct_name = None
    self.__vp_map = None

  def __remove_shader_constants(self):
    """Remove all unused shader constants.

    The method removes all shader constants from the code that are not
    references anywhere in the program file (not only the in the resulting
    shader program).

    The method updated the current program text associate with the
    preprocessor instance.

    Note: The method will invalidate the current input declaration.

    :Return:
      The method returns the shader program with all unused shader constants
      removed.
    """

    global verbose

    program = self.__program
    
    # Locate all shader constant declarations.
    class ShaderConst:
      def __init__(self, match):
	self.name = match.group(1)
	self.span = match.span()
	self.count = 0
    const_list = [ ]
    for match in re.finditer(
	r'^\s*const\s+[^\s]+\s+([a-zA-Z0-9_]+)\s*'
	r'(\[[^]]*\]\s*)*'
	r'(:\s*register\s*\([^)]*\)\s*)?;\s*$',
	program,
	re.MULTILINE):
      const_list.append(ShaderConst(match))

    # For every shader constant count the number of occurences in the shader
    # code. If the count is 1, then the shader constant is certainly unused.
    for const in const_list:
      for match in re.finditer(r'\b' + const.name + r'\b', program):
	const.count += 1

    # Remove all unused shader constants.
    const_list.reverse()
    for const in const_list:
      assert const.count > 0
      if const.count > 1: continue
      if verbose > 0:
	info('Removing unused shader constant ' + repr(const.name))
      program = program[0:const.span[0]] + '\n' + program[const.span[1]:]

    self.__input_decl = None
    self.__program = program
    return program

  def __strip_comments(program):
    """Strip all comments from the specified program.

    :Return:
      The program will all comments stripped.
    """

    program = re.sub(r'//[^\n]*\n', r'\n', program)
    program = re.sub(r'//[^\n]*$', r'\n', program)
    program = re.sub(r'/\*.*\*/', ' ', program)
    return program

  __strip_comments = staticmethod(__strip_comments)

  def __get_input_decl(self):
    """Extract a list of input declarations.

    The method updates the current input declaration of the preprocessor
    instance.

    :Return:
      A list of declarations or None in case of an error.  In input
      declaration is represented as a tuple (type, name, semantic).
    """

    program = self.__program
    entry = self.__entry
    match = re.search(
	r'^\s*[^\s]+\s+' + entry + r'\s*\(\s*([a-zA-Z0-9_]+)\s+IN\s*\)\s*$',
	program,
	re.MULTILINE);
    if match is None:
      error('Entry function ' + repr(entry) + ' not found')
      return None
    input_struct = match.group(1)
    match = re.search(
	r'\bstruct\s+' + input_struct + r'\s*{([^}]*)};',
	program)
    if match is None:
      error('Input structure ' + repr(input_struct) + ' not found')
      return None
    input_def = match.group(1).strip()
    input_def_position = match.start(1), match.end(1)
    decl_list = [ ]
    for decl in re.split(r'\s*;\s*', input_def):
      if decl == '': continue
      match = re.match(
	  r'^\s*([a-zA-Z0-9]+)\s+([a-zA-Z0-9_]+)\s*:\s*([a-zA-Z0-9_]+)\s*$',
	  decl)
      if match is None:
	error(
	    'Unrecognized input declaration ' + repr(decl)
	    + ' in structure ' + repr(input_struct))
	return None
      type = match.group(1)
      name = match.group(2)
      semantic = match.group(3)
      decl_list.append((type, name, semantic))

    self.__input_struct_name = input_struct
    self.__input_decl = input_def_position, input_def
    return decl_list

  def __normalize_semantic(semantic):
    """Normalize a semantic string.

    A nominalized string is of the form (prefix)(index), e.g. "POSITION0".
    The function will add an index "0" if it is missing.

    :Return:
      The function returns the nominalized semantic or None if the semantic
      string is not recognized.
    """

    semantic = semantic.upper()
    match = re.match(r'^([A-Z]+)([0-9]*)$', semantic)
    if match is None: return None
    prefix = match.group(1)
    index = match.group(2)
    if index == '': index = '0'
    semantic = prefix + str(int(index))
    return semantic

  __normalize_semantic = staticmethod(__normalize_semantic)

  def get_vp_attr_index(semantic):
    """Get the standard attibute index for the specified vertex program
    input semantic.

    :Return:
      The method returns the attibute index (in the range 0..15) or -1 if the
      semantic is not a standard semantic.
    """

    vp_semantics = PreProcess.__vp_semantics
    semantic = PreProcess.__normalize_semantic(semantic)
    for index in range(0, 16):
      if semantic in vp_semantics[index]: return index
    return -1

  get_vp_attr_index = staticmethod(get_vp_attr_index)

  def __get_input_semantics(self, normalize = True):
    """Extract a list of all shader input semantics.

    :Parameters:
      - `normalize`: Flag indicating if the semantics should be normalized.

    :Return:
      The method returns a list of all shader input semantics or None in case
      of an error.
    """

    decl_list = self.__get_input_decl()
    if decl_list is None: return None
    semantics_list = [ ]
    for decl in decl_list:
      if normalize:
	semantics_list.append(PreProcess.__normalize_semantic(decl[2]))
      else:
	semantics_list.append(decl[2])
    return semantics_list

  def __get_semantics_map(self):
    """Generate a semantics mapping.

    The method will map all non DX8 input semantics to unused semantics.
    
    :Return:
      The method returns the generated semantics mapping or None in case of
      an error.
    """

    semantics_list = self.__get_input_semantics(normalize = False)
    if semantics_list is None: return None
    used = [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]
    unmapped = [ ]
    for semantic in semantics_list:
      index = self.get_vp_attr_index(semantic)
      if index == -1 or used[index]:
	if semantic not in unmapped:
	  unmapped.append(semantic)
      else:
	used[index] = 1
    map = { }
    for semantic in unmapped:
      for index in range(0, 17):
	if not used[index]: break
      if index == 16:
	error('Input semantic ' + repr(semantic) + ' can not be mapped')
	return None
      map[semantic] = self.__vp_semantics[index][1]
      used[index] = 1
    return map

  def __remap_vp_input_semantics(self):
    """Re-map all unsupported semantics to unused semantics.

    The method will perform the substitution on the input structure
    declaration of the current program string.

    :Return:
      The method returns the semantics mapping applied to the program string
      or None in case of an error.
    """

    global verbose

    map = self.__get_semantics_map()
    if map is None: return None
    if len(map) == 0:
      self.__vp_map = { }
      return map
    input_def = self.__input_decl[1]
    for key in map:
      if verbose > 0:
	info('Remapping semantic ' + repr(key) + ' to ' + repr(map[key]))
      input_def = re.sub(r'\b' + key + r'\b', map[key], input_def)
    program = self.__program
    pos = self.__input_decl[0]
    program = program[:pos[0]] + input_def + program[pos[1]:]
    self.__program = program
    self.__vp_map = map
    return map

  def __is_vp(self):
    """Test if the specified profile indicates a vertex program.
    """

    profile = self.__profile
    if profile[:7] == 'sce_fp_':
      return False
    if profile[:7] == 'sce_vp_':
      return True
    if profile[:2] in ('fp', 'fs', 'ps'):
      return False
    if profile[:2] in ('vp', 'vs'):
      return True
    if profile.find('fp') != -1 or profile.find('ps') != -1:
      return False
    if profile.find('vp') != -1 or profile.find('vs') != -1:
      return True
    return False

  def run(self):
    """Run the preprocessor.

    If the program is a vertex program, then the preprocessor will
    subitute all unsupported input semantics.

    :Return:
      The method returns the final program string or None in case of an error.
    """

    global do_remap

    if self.__remove_shader_constants() is None: return None
    if do_remap and self.__is_vp():
      if self.__remap_vp_input_semantics() is None: return None
    return self.__program

  def get_program(self):
    """Get the processed program string.
    """

    return self.__program

  def get_input_program(self):
    """Get the original input program.
    """

    return self.__input_program

  def get_semantics_mapping(self):
    """Get the semantics mapping for vertex programs.
    """

    return self.__vp_map

def wrap_bin(bin, vp_map):
  """Wrap a binary into a wrapper including an optional semantics mapping.

  :Parameters:
    - `bin`: The binary program.
    - `vp_map`: The semantics mapping.  This may be None.

  :Return:
    The wrapped binary.
  """

  bin_hdr = len(bin)
  if vp_map is not None and len(vp_map) > 0:
    bin_hdr |= 0x80000000
    map = ''
    vp_rmap = { }
    for key in vp_map:
      index = PreProcess.get_vp_attr_index(vp_map[key])
      assert index != -1
      vp_rmap[index] = key
    for index in range(0, 16):
      if index in vp_rmap:
	map += vp_rmap[index] + ';'
      else:
	map += ';'
    debug('map = ' + repr(map))
    map_hdr = len(map) | 0x01000000;
    bin = struct.pack('>I', bin_hdr) + bin + struct.pack('>I', map_hdr) + map
  else:
    bin = struct.pack('>I', bin_hdr) + bin
  return bin

def compile(prog, params, comment):
  global cg_compiler, log_cg, log_bin
  global tmp_counter, basedir
  global extra_options
  global do_preprocess, do_wrap
  global verbose

  compiler_name = 'sce'
  if params[0] == '@':
    index = params.find(':')
    if index == -1:
      error('Invalid parameter string ' + repr(params))
      sys.exit(1)
    compiler_name = params[1:index]
    params = params[index + 1:]
  if compiler_name not in cg_compiler:
    error('Compiler ' + repr(compiler_name) + ' not supported')
    sys.exit(1)
  compiler = cg_compiler[compiler_name]

  prog_filename = tmpnam() + '.cg'
  bin_filename = tmpnam() + '.bin'

  options = ''
  param_list = re.split('\s*,\s*', params)
  profile = None
  entry = None
  for param in param_list + extra_options:
    if len(param) == 0: continue
    if param[0] == '-':
      options += ' ' + param
      if param[:8] == '-profile':
	try:
	  profile = param.split(' ')[1]
	except:
	  profile = None
      if param[:6] == '-entry':
	try:
	  entry = param.split(' ')[1]
	except:
	  entry = None
    else:
      options += ' -D' + param
  if entry is None: entry = 'main'

  vp_map = None
  if do_preprocess:
    preprocess = PreProcess(prog, entry, profile)
    prog = preprocess.run()
    if prog is None:
      info('Writing input Cg file to ' + repr(prog_filename))
      f = file(prog_filename, 'w')
      f.write(preprocess.get_input_program())
      f.close()
      return None
    vp_map = preprocess.get_semantics_mapping()

  prog_file = file(prog_filename, 'w')
  prog_file.write(prog)
  prog_file.close()

  if log_cg:
    log_cg_filename = ''
    if basedir is not None:
      log_cg_filename += basedir + '/'
    if profile is not None:
      log_cg_filename += profile
      if not os.path.exists(log_cg_filename):
	os.makedirs(log_cg_filename)
      log_cg_filename += '/'
    if comment is not None and len(comment) > 0:
      log_cg_filename += comment + '_'
    log_cg_filename += str(tmp_counter) + '.cg'
    info('Logging Cg file to ' + repr(log_cg_filename))
    f = file(log_cg_filename, 'w')
    f.write(prog)
    f.close()

  command = (
      compiler
      + ' ' + options
      + ' -o ' + bin_filename
      + ' ' + prog_filename)
  info('Compiling... [' + compiler_name + ']')
  if verbose > 1:
    info('Cg options: ' + repr(options))
  if len(comment) > 0: debug('Comment: ' + comment)
  cmd_out = os.popen(command, 'r')
  cmd_output = cmd_out.read()
  cmd_exit = cmd_out.close()
  if cmd_exit:
    error('Compiler error (exit code ' + repr(cmd_exit) + ')\n'
	+ '  Cg program: ' + repr(prog_filename) + '\n'
	+ '  Command: ' + repr(command) + '\n'
	+ '  Command output: ' + cmd_output + '\n')
    bin = None
  else:
    bin_file = file(bin_filename)
    bin = bin_file.read()
    bin_file.close()

    os.remove(prog_filename)
    os.remove(bin_filename)

  if bin is not None and log_bin:
    log_bin_filename = ''
    if basedir is not None:
      log_bin_filename += basedir + '/'
    if profile is not None:
      log_bin_filename += profile
      if not os.path.exists(log_bin_filename):
	os.makedirs(log_bin_filename)
      log_bin_filename += '/'
    if comment is not None and len(comment) > 0:
      log_bin_filename += comment + '_'
    log_bin_filename += str(tmp_counter) + '.bin'
    info('Logging binary file to ' + repr(log_bin_filename))
    f = file(log_bin_filename, 'w')
    f.write(bin)
    f.close()

  if bin is not None and do_wrap:
    bin = wrap_bin(bin, vp_map)

  return bin

def recvall(conn, count):
  buffer = ''
  while len(buffer) < count:
    buffer += conn.recv(count - len(buffer))
  return buffer

def serve(conn, addr, counter):
  global verbose, do_simplify

  if verbose > 0:
    info('Incoming connection from ' + repr(addr))
  info('Request #' + str(counter))
  buf = conn.recv(12)
  len_params, len_comment, len_prog = struct.unpack('>III', buf)
  params = recvall(conn, len_params)
  comment = recvall(conn, len_comment)
  prog = recvall(conn, len_prog)

  bin = compile(prog, params, comment)
  if bin is None:
    prog_simple = simplify_stage1(prog)
    warning('Trying a simplified program '
	+ '(the resulting shader may be sub-optimal)')
    bin = compile(prog_simple, params, comment)
  if bin is None:
    prog_simple = simplify_stage2(prog_simple)
    warning('Trying an even more simplified program '
	+ '(the resulting shader may be broken)')
    bin = compile(prog_simple, params, comment)
  if bin is None: bin = ''

  debug('Sending response (' + str(len(bin)) + ' bytes)')
  conn.sendall(bin)
  conn.close()

def simplify_stage1(prog):
  """Simplify the specified program.

  The function will replace all 'out' parameters with 'inout'
  parameters.
  """

  return re.sub(r'([(,])\s*out\s', r'\1inout ', prog)

def simplify_stage2(prog):
  """Simplify the specified program even more.

  The function will remove all variable initializers of the form
  'type VAR = (type)0;'.  The SONY Cg compiler (as well as the NVIDIA
  compiler) will not allow such initializations if the structure type
  contains an array.
  """

  return re.sub(
      r'([a-zA-Z0-9]+)\s+([a-zA-Z0-9]+)\s*=\s*\(\1\)0\s*;',
      r'\1 \2;',
      prog)

def main():
  global srv_port, basedir, verbose, log_cg, log_bin, extra_options
  global do_wrap, do_remap, do_simplify

  opts, args = getopt.getopt(
      sys.argv[1:],
      'hVvsd:p:x:',
      [ 'help', 'version', 'verbose', 'silent', 'dir=', 'port=',
	'log-cg', 'log-bin', 'nowrap', 'noremap', 'simplify', 'extra=' ])
  for a, o in opts:
    if a in ('-h', '--help'):
      version(sys.stdout)
      help(sys.stdout)
      sys.exit(0)
    elif a in ('-V', '--version'):
      version(sys.stdout)
      sys.exit(0)
    elif a in ('-v', '--verbose'):
      verbose += 1
    elif a in ('-d', '--dir'):
      basedir = o
    elif a in ('-p', '--port'):
      srv_port = int(o)
    elif a in ('--log-cg'):
      log_cg = True
    elif a in ('--log-bin'):
      log_bin = True
    elif a in ('--nowrap'):
      do_wrap = False
    elif a in ('--noremap'):
      do_remap = False
    elif a in ('--simplify'):
      do_simplify = True
    elif a in ('-x', '--extra'):
      extra_options.append(o)
    else:
      error('Unrecognized option ' + repr(a))

  init()
  srv = None
  while True:
    if srv is not None:
      try:
	srv.close()
      except socket.error, e:
	error('socket error in close(): ' + str(e), e)
      srv = None
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
      srv.bind(('0.0.0.0', srv_port))
    except socket.error, e:
      error('socket error in bind(): ' + str(e), e)
      time.sleep(1.0)
      continue
    srv.listen(5)
    counter = 0
    try:
      while True:
	counter += 1
	conn, addr = srv.accept()
	serve(conn, addr, counter)
    except socket.error, e:
	    error('socket error: ' + str(e), e)
	    time.sleep(1.0)

if __name__ == '__main__': main()

# vim: sw=2 ts=8 tw=78

