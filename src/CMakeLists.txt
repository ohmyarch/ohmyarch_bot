include_directories(${CASABLANCA_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)

add_executable(ohmyarch_bot
  main.cc
  joke.cc
  quote.cc
  funny_pics.cc
  message.cc
  config.cc
)

target_link_libraries(ohmyarch_bot
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${CASABLANCA_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS ohmyarch_bot RUNTIME DESTINATION bin)
