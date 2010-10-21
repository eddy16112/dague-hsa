# 
# DAGuE Internal: generation of various floating point precision files from a template.
#



#
# Generates a rule for every SOURCES file, to create the precisions in PRECISIONS. 
# A new file is created, from a copy by default
# If the first precision is "/", all occurences of the basename in the file are remplaced by 
# "pbasename" where p is the selected precision. 
# the target receives a -DPRECISION_p in its cflags. 
#
macro(precisions_rules OUTPUTLIST PRECISIONS SOURCES)
 set(precisions_rules_SED 0)
 foreach(prec_rules_SOURCE ${SOURCES})
  string(REGEX REPLACE "^(.*/)*(.+)\\.(.+)$" "\\2;\\3" prec_rules_BSRCl ${prec_rules_SOURCE})
  set(prec_rules_BSRC "${CMAKE_MATCH_2}")
  set(prec_rules_ESRC "${CMAKE_MATCH_3}")
  foreach(prec_rules_PREC ${PRECISIONS})
   if("${prec_rules_PREC}" MATCHES "/")
    set(precisions_rules_SED 1)
   else()
    set(prec_rules_OSRC "generated/${prec_rules_PREC}${prec_rules_BSRC}.${prec_rules_ESRC}")
    if(precisions_rules_SED)
      add_custom_command(
        OUTPUT "${prec_rules_OSRC}"
        COMMAND sed '{ s/${prec_rules_BSRC}/${prec_rules_PREC}${prec_rules_BSRC}/g }' ${CMAKE_CURRENT_SOURCE_DIR}/${prec_rules_SOURCE} >${CMAKE_CURRENT_BINARY_DIR}/${prec_rules_OSRC}
        MAIN_DEPENDENCY ${prec_rules_SOURCE}
        DEPENDS ${DAGUEPP})
    else()
      add_custom_command(
        OUTPUT "${prec_rules_OSRC}"
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/${prec_rules_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/${prec_rules_OSRC}
        MAIN_DEPENDENCY ${prec_rules_SOURCE}
        DEPENDS ${DAGUEPP})
    endif()
    set_source_files_properties(${prec_rules_OSRC} PROPERTIES COMPILE_FLAGS "-DPRECISION_${prec_rules_PREC}")
    list(APPEND ${OUTPUTLIST} ${prec_rules_OSRC})
   endif()
  endforeach()
 endforeach()
endmacro(precisions_rules)

