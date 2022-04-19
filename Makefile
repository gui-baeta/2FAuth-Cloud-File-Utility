AS_TARGET = AS
FS_TARGET = FS
PD_TARGET = pd
USER_TARGET = user

AS_BIN = AS_/AS.o
FS_BIN = FS_/fs_server.o
PD_BIN = pd_/pd_client.o
USER_BIN = user_/UserApp.o

COMPILER = g++
FLAGS = -std=c++17 -g -lpthread -pthread

.Phony: $(AS_TARGET) $(FS_TARGET) $(PD_TARGET) $(USER_TARGET)

build: $(AS_TARGET) $(FS_TARGET) $(PD_TARGET) $(USER_TARGET)

$(AS_TARGET): $(AS_BIN)
	$(COMPILER) $(FLAGS) $^ -o $@

$(AS_BIN): $(AS_BIN:%.o=%.cpp) $(AS_BIN:%.o=%.hpp)
	$(COMPILER) $(FLAGS) -o $(AS_BIN) -c $(AS_BIN:%.o=%.cpp)

$(FS_TARGET): $(FS_BIN)
	$(COMPILER) $(FLAGS) $^ -o $@

$(FS_BIN): $(FS_BIN:%.o=%.cpp) $(FS_BIN:%.o=%.hpp)
	$(COMPILER) $(FLAGS) -o $(FS_BIN) -c $(FS_BIN:%.o=%.cpp)

$(PD_TARGET): $(PD_BIN)
	$(COMPILER) $(FLAGS) $^ -o $@

$(PD_BIN): $(PD_BIN:%.o=%.cpp) $(PD_BIN:%.o=%.hpp)
	$(COMPILER) $(FLAGS) -o $(PD_BIN) -c $(PD_BIN:%.o=%.cpp)

$(USER_TARGET): $(USER_BIN)
	$(COMPILER) $(FLAGS) $^ -o $@

$(USER_BIN): $(USER_BIN:%.o=%.cpp)
	$(COMPILER) $(FLAGS) -o $(USER_BIN) -c $(USER_BIN:%.o=%.cpp)

clean:
	rm $(AS_TARGET) $(FS_TARGET) $(PD_TARGET) $(USER_TARGET) $(AS_BIN) $(FS_BIN) $(PD_BIN) $(USER_BIN) vgcore.*
