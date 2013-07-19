# -*- mode: python -*-

Import("env")

env = env.Clone()
env.Append(CPPPATH=[Dir('.')])
name = 'backup_plugin'
plugin = env.SharedLibrary(name, ['backup_plugin.cpp',
                                  'manager.cpp'])
Return('plugin', 'name')
