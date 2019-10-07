
set -e

./build/bin/hlms garg.mdl build/asdf 1000x1600 0 1 1

echo "Creating thumbnails"
convert build/asdf000.png -resize 20x32 asdf_tiny_temp.png
~/mediancut-posterizer/posterize 8 asdf_tiny_temp.png asdf_tiny.png
pngcrush -ow asdf_tiny.png
rm asdf_tiny_temp.png
stat asdf_tiny.png

convert build/asdf000.png -resize 125x200 asdf_small_temp.png
~/mediancut-posterizer/posterize 16 asdf_small_temp.png asdf_small.png
pngcrush -ow asdf_small.png
rm asdf_small_temp.png

convert build/asdf000.png -resize 500x800 asdf_large_temp.png
~/mediancut-posterizer/posterize 255 asdf_large_temp.png asdf_large.png
pngcrush -ow asdf_large.png
rm asdf_large_temp.png

exit

#echo "Creating gif..."
#convert -delay 3 *.tga -layers Optimize asdf.gif

echo "Creating webm..."
# crf = 0-63 (good-bad quality)
ffmpeg -y -r 30 -f image2 -i build/asdf%03d.png -pix_fmt yuva420p -c:v libvpx-vp9 -b:v 0 -crf 30 -pass 1 -an -f webm /dev/null
ffmpeg -y -r 30 -f image2 -i build/asdf%03d.png -pix_fmt yuva420p -c:v libvpx-vp9 -b:v 0 -crf 30 -pass 2 -an asdf.webm
