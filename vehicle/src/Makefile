all: install format build

setup: install

build: vehicle

format:
	find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.ino" -exec clang-format -i {} +
	[ -n "$(find . -name '*.js' -o -name '*.ts')" ] && prettier --write "**/*.js" "**/*.ts" || true

install:
	sudo apt-get update
	sudo apt-get remove -y libnode-dev nodejs npm
	sudo apt-get autoremove -y
	sudo apt-get install -y curl
	curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
	sudo apt-get install -y nodejs
	# npmがインストールされていない場合のフォールバック
	if ! command -v npm >/dev/null 2>&1; then sudo apt-get install -y npm; fi
	sudo npm install -g prettier
	sudo apt-get install -y build-essential libopencv-dev
	sudo apt install clang-format
	# Download OpenPose MobileNet model (TensorFlow)
	wget -nc https://raw.githubusercontent.com/quanhua92/human-pose-estimation-opencv/master/graph_opt.pb

CXX := g++
OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS := $(shell pkg-config --libs opencv4)
CXXFLAGS := -Wall -Wextra -std=c++17 $(OPENCV_CFLAGS)
LDFLAGS := $(OPENCV_LIBS)

SRC_DIR := vehicle/target
BUILD_DIR := vehicle/build

SRCS := $(shell find $(SRC_DIR) -name "*.cpp")
EXECUTABLES := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%,$(SRCS))

vehicle: $(EXECUTABLES)

$(BUILD_DIR)/%: $(SRC_DIR)/%.cpp
	@echo "Compiling and Linking $<..."
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
	@echo "Build finished: $@"

clean:
	@echo "Cleaning up..."
	rm -rf $(BUILD_DIR)

.PHONY: all format install vehicle clean build
