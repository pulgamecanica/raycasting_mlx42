# -----------------------
# Project settings
# -----------------------
NAME = demo

# -----------------------
# Compiler Settings
# -----------------------
CC = cc
CFLAGS = -O3 -Wall -Wextra -Werror -Iinclude -IMLX42/include -Iinclude/gui

# -----------------------
# Sources settings
# -----------------------
SRCS = $(wildcard src/*.c src/gui/*.c)
OBJS = $(SRCS:.c=.o)

# -----------------------
# Libraries/Linkage settings
# -----------------------
MLX_FLAGS = -LMLX42/build -lmlx42 -ldl -lglfw -lm -lpthread
MLX_NATIVE_LIB = MLX42/build/libmlx42_native.a
MLX_WEB_LIB = MLX42/build_web/libmlx42_web.a

# -----------------------
# Web settings
# -----------------------
WEB = web/demo.js

all: $(MLX_NATIVE_LIB) $(MLX_WEB_LIB) $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME) $(MLX_FLAGS)

$(MLX_NATIVE_LIB):
	cd MLX42 && cmake -B build
	cd MLX42 && cmake --build build -j4
	mv MLX42/build/libmlx42.a $(MLX_NATIVE_LIB)

$(MLX_WEB_LIB):
	cd MLX42 && emcmake cmake -B build_web
	cd MLX42 && cmake --build build_web --parallel
	mv MLX42/build_web/libmlx42.a $(MLX_WEB_LIB)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

web: $(WEB)

$(WEB): $(SRCS) $(MLX_WEB_LIB)
	mkdir -p web
	emcc -DWEB -O3 -I include -Iinclude/gui -I MLX42/include -pthread $(SRCS) \
		-o $(WEB) \
		$(MLX_WEB_LIB) \
		-s USE_GLFW=3 -s USE_WEBGL2=1 -s FULL_ES3=1 -s WASM=1 \
		-s NO_EXIT_RUNTIME=1 -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
		-s ALLOW_MEMORY_GROWTH \
		--preload-file assets

re: fclean all

.PHONY: all clean fclean re $(WEB)
