#/bin/bash -e

if [ -d build ]; then
        rm -rf build
fi

mkdir build
cd build
cmake ..
make -j8 package
mv *.deb ../../..
cd ..
rm -rf build
