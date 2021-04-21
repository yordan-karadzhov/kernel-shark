if cmake -DCOMPONENT=libkshark-devel -P cmake_install.cmake; then
    echo "libkshark-devel installed correctly"
else
        exit 1
fi
