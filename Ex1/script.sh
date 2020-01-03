#!/bin/bash
mkdir my_id # Create a directory with your ID as its name
cd ./my_id # Set it as the current directory
mkdir temp # Create another directory inside it called “temp”
echo fname > ./temp/fname # In "temp", create 3 files – one named as your first name
echo lname > ./temp/lname # One as your last name
echo fnamelname > ./temp/fnamelname # And one as your TAU username
cp ./temp/fname ./lname # Copy "first name" file to the original directory and rename it to "last name"
cp ./temp/lname ./fname # Copy "last name" file to the original directory and rename it to "first name"
rm ./temp/fname # Delete the original 2 files in "temp"
rm ./temp/lname # Delete the original 2 files in "temp"
mv ./temp/fnamelname ./fnamelname # Move the 3 rd file to the original directory
rmdir ./temp # Delete the temp directory
echo "Folder my_id content:"
ls -ahl
for f in *; do
	echo "File: ./my_id/$f 		Content: $(cat $f)"
done
