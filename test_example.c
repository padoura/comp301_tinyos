#include <stdio.h>
#include <string.h>
#include "unit_testing.h"

BARE_TEST(my_test, "This is a silly test")
{
  ASSERT(1+1==2);
  ASSERT(2*2*2 < 10);
}

BARE_TEST(strcat_test, "this is a test for function strcat"){

  char name[60] = "Michalis";
  strcat(name, " Pantourakis");
  ASSERT(strcmp("Michalis Pantourakis", name)==0);

};

TEST_SUITE(all_my_tests, "These are mine")
{
  &my_test,
  &strcat_test,
  NULL
};

int main(int argc, char** argv)
{
  return register_test(&all_my_tests) ||
    run_program(argc, argv, &all_my_tests);
}

