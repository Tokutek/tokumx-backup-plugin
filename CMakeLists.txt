include_directories(.)

add_library(backup_plugin SHARED
  backup_plugin
  manager
  )

install(TARGETS backup_plugin
  DESTINATION ${INSTALL_LIBDIR}/plugins
  COMPONENT tokumx_plugins
  )