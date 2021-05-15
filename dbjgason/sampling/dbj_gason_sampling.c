

int pretty_print(int argc, char** argv);
int test_suite(const int argc, char** argv);

extern int system(const char*);

int main(const int argc, char** argv)
{
	test_suite(argc, argv);
	pretty_print(argc, argv);

#ifdef _WIN32
	system("pause");
#endif
}