find_package(Waffle REQUIRED)

set(BARGRAPH_TEST_SOURCE
  main_test.cpp
  test_bargraph.cpp
  test_selection.cpp
  test_selection.hpp
  )


add_executable( test_amd
  test_counter.cpp
  )

target_link_libraries(test_amd
  waffle-1
  GL
  epoxy
  dl
  )

