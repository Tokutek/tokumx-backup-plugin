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

if (TOKUMX_ENTERPRISE_CREATE_EXPORTS)
  file(RELATIVE_PATH _relative_source_dir "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  install(TARGETS backup_plugin DESTINATION "${_relative_source_dir}" COMPONENT tokumx_enterprise_exports EXPORT tokumx_enterprise_exports)
  exports_install(backup_plugin "\${INSTALL_LIBDIR}/plugins" tokumx_plugins)
  unset(_relative_source_dir)
endif ()
