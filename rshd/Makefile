TARGET_NAME=rshd

USE_CPP=$(shell test -e "$(TARGET_NAME).cpp" && echo "CPP")
all: $(TARGET_NAME)

ifeq ($(USE_CPP),CPP)
${TARGET_NAME} : $(TARGET_NAME).cpp
	$(CXX) -std=c++11 -Wall -o $@ $<
else
${TARGET_NAME} : $(TARGET_NAME).c
	$(CC) -o $@ $<
endif
clean :
	$(RM) ${TARGET_NAME}
