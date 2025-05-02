# C 编译器
CC = gcc
# C++ 编译器
CXX = g++
# 编译和链接选项，添加 -I 以包含 liburing 头文件路径，添加 -luring 链接 liburing 库，添加 -lmymuduo 链接 mymuduo 库
FLAGS = -I ./NtyCo/core/ -L ./NtyCo/ -lntyco -lpthread -ldl -I/usr/local/include -luring -lmymuduo
# C 源文件列表
SRCS  =  kvstore_array.c epoll_entry.c  nty_entry.c  kvstore_rbtree.c kvstore_hash.c io_uring_entry.c
# C++ 源文件
CXX_SRCS = kmuduo_entry.cc kvstore.cc
# 测试用例源文件
TESTCASE_SRCS = testcase.c

# 目标可执行文件
TARGET   = kvstore
# 子目录
SUBDIR = ./NtyCo/
# 测试用例可执行文件
TESTCASE = testcase

# 生成 C 目标文件列表
OBJS = $(SRCS:.c=.o)
# 生成 C++ 目标文件列表
CXX_OBJS = $(CXX_SRCS:.cc=.o)

# 所有目标
all: $(SUBDIR) $(TARGET) $(TESTCASE)

# 编译子目录
$(SUBDIR): ECHO
	make -C $@
ECHO:
	@echo $(SUBDIR)

# 链接生成目标可执行文件
$(TARGET): $(OBJS) $(CXX_OBJS)
	$(CXX) -o $@ $^ $(FLAGS)

# 编译测试用例
$(TESTCASE): $(TESTCASE_SRCS)
	$(CC) -o $@ $^ $(FLAGS)

# 编译 C 源文件生成目标文件
%.o: %.c
	$(CC) $(FLAGS) -c $^ -o $@

# 编译 C++ 源文件生成目标文件
%.o: %.cc
	$(CXX) $(FLAGS) -c $^ -o $@

# 清理生成的文件
clean:
	rm -rf $(OBJS) $(CXX_OBJS) $(TARGET) $(TESTCASE)