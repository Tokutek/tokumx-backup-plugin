include_directories(.)

add_library(backup_plugin SHARED
  backup_plugin
  manager
  )
add_dependencies(backup_plugin install_tdb_h)

install(TARGETS backup_plugin
  DESTINATION ${INSTALL_LIBDIR}/plugins
  COMPONENT tokumx_plugins
  )