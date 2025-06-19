cmake_minimum_required(VERSION 3.15)

project(Seabattle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Ищем Boost (подключается через ConanDeps)
find_package(Boost REQUIRED)

# Подключаем потоки (для macOS — это тоже нужно)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Создаём исполняемый файл
add_executable(seabattle src/main.cpp src/seabattle.h)

# Линкуем Boost и потоки
target_link_libraries(seabattle PRIVATE Boost::boost Threads::Threads)
