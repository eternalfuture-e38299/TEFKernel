.PHONY: all clean help release debug android windows linux macos

# 定义所有预设
PRESETS = \
    linux-x86_64-debug linux-x86_64-release \
    linux-x86-debug linux-x86-release \
    windows-x86_64-debug windows-x86_64-release \
    windows-x86-debug windows-x86-release \
    macos-x86_64-debug macos-x86_64-release \
    macos-arm64-debug macos-arm64-release \
    android-arm64-debug android-arm64-release \
    android-arm32-debug android-arm32-release

# 按类型分组
RELEASE_PRESETS = $(filter %-release,$(PRESETS))
DEBUG_PRESETS = $(filter %-debug,$(PRESETS))

# 按平台分组
ANDROID_PRESETS = $(filter android-%,$(PRESETS))
WINDOWS_PRESETS = $(filter windows-%,$(PRESETS))
LINUX_PRESETS = $(filter linux-%,$(PRESETS))
MACOS_PRESETS = $(filter macos-%,$(PRESETS))

# 组合目标
android-release: $(filter android-%-release,$(PRESETS))
android-debug: $(filter android-%-debug,$(PRESETS))
windows-release: $(filter windows-%-release,$(PRESETS))
windows-debug: $(filter windows-%-debug,$(PRESETS))
linux-release: $(filter linux-%-release,$(PRESETS))
linux-debug: $(filter linux-%-debug,$(PRESETS))
macos-release: $(filter macos-%-release,$(PRESETS))
macos-debug: $(filter macos-%-debug,$(PRESETS))

# x86_64 和 ARM64 分别编译
x86_64: $(filter %-x86_64-%,$(PRESETS))
arm64: $(filter %-arm64-%,$(PRESETS))
x86: $(filter %-x86-%,$(PRESETS))
arm32: $(filter %-arm32-%,$(PRESETS))

# 编译所有
all: $(PRESETS)

# 只编译 Release 版本
release: $(RELEASE_PRESETS)

# 只编译 Debug 版本
debug: $(DEBUG_PRESETS)

# 按平台编译
android: $(ANDROID_PRESETS)
windows: $(WINDOWS_PRESETS)
linux: $(LINUX_PRESETS)
macos: $(MACOS_PRESETS)

# 通用编译规则
$(PRESETS):
	@echo "=========================================="
	@echo "Building: $@"
	@echo "=========================================="
	@cmake --preset $@ && cmake --build --preset $@
	@if [ $$? -eq 0 ]; then \
		echo "✓ Successfully built: $@"; \
	else \
		echo "✗ Failed to build: $@"; \
		exit 1; \
	fi
	@echo ""

# 并行编译所有（使用 -j 参数）
parallel:
	@for preset in $(PRESETS); do \
		echo "Building $$preset &"; \
		cmake --preset $$preset && cmake --build --preset $$preset & \
	done; \
	wait

# 清理
clean:
	@echo "Cleaning all build directories..."
	@rm -rf build
	@echo "✓ Clean complete"

# 查看帮助
help:
	@echo "Available targets:"
	@echo "  all                     - Build all configurations"
	@echo "  release                 - Build all Release configurations"
	@echo "  debug                   - Build all Debug configurations"
	@echo "  android                 - Build all Android configurations"
	@echo "  android-release         - Build all Android Release configurations"
	@echo "  android-debug           - Build all Android Debug configurations"
	@echo "  windows                 - Build all Windows configurations"
	@echo "  windows-release         - Build all Windows Release configurations"
	@echo "  windows-debug           - Build all Windows Debug configurations"
	@echo "  linux                   - Build all Linux configurations"
	@echo "  linux-release           - Build all Linux Release configurations"
	@echo "  linux-debug             - Build all Linux Debug configurations"
	@echo "  macos                   - Build all macOS configurations"
	@echo "  macos-release           - Build all macOS Release configurations"
	@echo "  macos-debug             - Build all macOS Debug configurations"
	@echo "  x86_64                  - Build all x86_64 configurations"
	@echo "  x86                     - Build all x86 configurations"
	@echo "  arm64                   - Build all ARM64 configurations"
	@echo "  arm32                   - Build all ARM32 configurations"
	@echo "  parallel                - Build all configurations in parallel"
	@echo "  clean                   - Remove all build directories"
	@echo ""
	@echo "Individual targets:"
	@printf "  %s\n" $(PRESETS)
