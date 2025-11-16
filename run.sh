rm -rf build/Nas_Security_App*

sudo docker run --rm -v ./:/app ignislee/alpine319-static-compiler:1.1 sh -c "chmod +x /app/build.sh && /app/build.sh --release"

cd build/
