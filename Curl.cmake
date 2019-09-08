##---------------------------------------------------------------------------
## Author:      Pavel Kalian, Sean D'Epagnier
## Copyright:   
## License:     GPLv3+
##---------------------------------------------------------------------------

# configure curl library

IF(WIN32)
  IF(MSVC)
    SET(CURL_LIBRARIES "${CMAKE_SOURCE_DIR}/buildwin/libcurl.lib")
    #INSTALL(FILES "buildwin/libcurl.dll" DESTINATION ".")
    #INSTALL(FILES "buildwin/libeay32.dll" DESTINATION ".")
    #INSTALL(FILES "buildwin/ssleay32.dll" DESTINATION ".")
    #INSTALL(FILES "buildwin/zlib1.dll" DESTINATION ".")
    #INSTALL(FILES "buildwin/curl-ca-bundle.crt" DESTINATION ".")
    INCLUDE_DIRECTORIES(src/wxcurl)
    INCLUDE_DIRECTORIES(src/wxcurl/include)

  ELSE(MSVC) ## mingw
    FIND_PACKAGE(CURL REQUIRED)
    INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIRS})
  ENDIF(MSVC)
    
  #TARGET_LINK_LIBRARIES(${PACKAGE_NAME} ${CURL_LIBRARIES})
ENDIF(WIN32)

IF(UNIX)
    FIND_PACKAGE(CURL REQUIRED)
    INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIR})
    #TARGET_LINK_LIBRARIES( ${PACKAGE_NAME} ${CURL_LIBRARIES} )
ENDIF(UNIX)
