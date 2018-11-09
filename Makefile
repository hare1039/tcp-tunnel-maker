
ALL:
	clang++ *.cpp -glldb -o run -std=c++17 -fcoroutines-ts -lboost_program_options -lboost_system

.PHONY: release
release:
	clang++ *.cpp -o release -std=c++17 -fcoroutines-ts -lboost_program_options -lboost_system -O3 -DNDEBUG
