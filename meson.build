project('WaylandServer', 'c', 'cpp',
	version : '0.1',
	default_options : ['warning_level=3', 'cpp_std=c++2a']
)

add_project_arguments('-Wno-unused-parameter', language: 'cpp')
add_project_arguments('-Wno-unused-variable', language: 'cpp')

cpp = meson.get_compiler('cpp')

arch = build_machine.cpu_family()
if arch == 'aarch64'
	arch = 'arm64'
endif

wlserverdep    = dependency('wayland-server')
wlprotocolsdep = dependency('wayland-protocols')

wl_dir = wlserverdep.get_variable('pkgdatadir')
wl_protocol_dir = wlprotocolsdep.get_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(wayland_scanner_dep.get_variable('wayland_scanner'), native: true)

protocols = {
	# Stable upstream protocols
	'xdg-shell': wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	'presentation-time': wl_protocol_dir / 'stable/presentation-time/presentation-time.xml',
	'viewporter': wl_protocol_dir / 'stable/viewporter/viewporter.xml',
	'activation-v1': wl_protocol_dir / 'staging/xdg-activation/xdg-activation-v1.xml',
	'text-input-unstable-v3': wl_protocol_dir / 'unstable/text-input/text-input-unstable-v3.xml',
	'server-decoration': 'protocols/server-decoration.xml',
}

wl_files = []
protocols_code = {}
protocols_server_header = {}
protocols_client_header = {}
foreach name, path : protocols
	code = custom_target(
		name.underscorify() + '_c',
		input: path,
		output: '@BASENAME@-protocol.c',
		command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
	)
	wl_files += code

	server_header = custom_target(
		name.underscorify() + '_server_h',
		input: path,
		output: '@BASENAME@-protocol.h',
		command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
	)
	wl_files += server_header

	client_header = custom_target(
		name.underscorify() + '_client_h',
		input: path,
		output: '@BASENAME@-client-protocol.h',
		command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
		build_by_default: false,
	)

	protocols_code += { name: code }
	protocols_server_header += { name: server_header }
	protocols_client_header += { name: client_header }
endforeach

protocols_cpp = {
	'Wayland': wl_dir / 'wayland.xml',
	'XdgShell': wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
	'PresentationTime': wl_protocol_dir / 'stable/presentation-time/presentation-time.xml',
	'Viewporter': wl_protocol_dir / 'stable/viewporter/viewporter.xml',
	'XdgActivationV1': wl_protocol_dir / 'staging/xdg-activation/xdg-activation-v1.xml',
	'TextInputUnstableV3': wl_protocol_dir / 'unstable/text-input/text-input-unstable-v3.xml',
	'ServerDecoration': 'protocols/server-decoration.xml',
}

foreach name, path : protocols_cpp
	code = custom_target(
		name + '_cpp',
		input: path,
		output: name + '.cpp',
		command: [wayland_scanner, 'private-code-cpp', '@INPUT@', '@OUTPUT@'],
	)
	wl_files += code

	server_header = custom_target(
		name + '_h',
		input: path,
		output: name + '.h',
		command: [wayland_scanner, 'server-header-cpp', '@INPUT@', '@OUTPUT@'],
	)
	wl_files += server_header
endforeach


shared_library('wayland-server-inproc',
	[
		'WaylandServer.cpp',
		'WlResource.cpp',
		'WlGlobal.cpp',
		'HaikuShm.cpp',
		'HaikuCompositor.cpp',
		'HaikuSubcompositor.cpp',
		'HaikuViewporter.cpp',
		'HaikuXdgShell.cpp',
		'HaikuXdgPositioner.cpp',
		'HaikuXdgSurface.cpp',
		'HaikuXdgToplevel.cpp',
		'HaikuXdgPopup.cpp',
		'HaikuServerDecoration.cpp',
		'HaikuOutput.cpp',
		'HaikuSeat.cpp',
		'HaikuTextInput.cpp',
		'XkbKeymap.cpp',
		'HaikuDataDeviceManager.cpp',
		wl_files
	],
	name_prefix: '',
	include_directories: [
		'/boot/system/develop/headers/private/shared',
		'/boot/system/develop/headers/private/interface',
		'/boot/system/develop/headers/private/system',
		'/boot/system/develop/headers/private/system/arch/' + arch,
	],
	dependencies: [
		cpp.find_library('be'),
		wlserverdep,
	],
	gnu_symbol_visibility: 'hidden',
	install : true
)
