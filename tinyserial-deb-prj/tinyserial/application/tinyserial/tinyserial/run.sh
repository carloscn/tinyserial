echo "Run TinySerial Tool as root mode."
cd /opt/tinyserial
export LD_LIBRARY_PATH=/opt/tinyserial:$LD_LIBRARY_PATH
./tinyserial
