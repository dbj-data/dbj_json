#include <stdlib.h>

static char TEST_FILE[] =
    "C:\\Users\\dusan\\source\\repos\\dbj_json\\data\\fathers.json ";

extern int pretty_print(int, char *[]);
extern int test_suite(const int, char *[]);

int main(const int argc, char *argv[]) {

  // char *(*dbj_argv)[2] = malloc(sizeof(char *[2]));

  char *dbj_argv[2] = {argv[0], &TEST_FILE[0]};

  test_suite(2, dbj_argv);
  pretty_print(2, dbj_argv);

  // free(dbj_argv);

#ifdef _WIN32
  system("pause");
#endif
}