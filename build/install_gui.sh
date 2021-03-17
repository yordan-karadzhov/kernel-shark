if cmake -DCOMPONENT=kernelshark -P cmake_install.cmake; then
    echo "Kernelshark installed correctly"
else
    exit 1
fi

if ! cmake -DCOMPONENT=polkit-policy -P cmake_install.cmake; then
    echo >&2 "Warning: polkit policy not installed"
fi
