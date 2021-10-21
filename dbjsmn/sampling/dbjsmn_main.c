

int simple(const int argc, char **argv);
int tests(const int argc, char **argv);
int jsondump(int argc, char **argv);

int main(const int argc, char **argv) {
  simple(argc, argv);
  tests(argc, argv);
  jsondump(argc, argv);
  return 0;
}
