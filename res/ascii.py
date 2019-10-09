import sys
from PIL import Image

ascii_chars = list("#;:\",`'. ")   #用来替代像素的字符集合...

def get_chars(r, g, b, alpha = 256):
    global ascii_chars
    if alpha == 0:
        return ' '
    length = len(ascii_chars)
    gray = int(0.3333 * r + 0.3333 * g + 0.3333 * b)
    unit = alpha / length                 #将256个像素均分给字符...
    return ascii_chars[int(gray/unit)]


outPutHeight = int(sys.argv[3])
outPutWidth = int(sys.argv[2])


img = Image.open(sys.argv[1]);
img = img.resize((outPutWidth, outPutHeight))


txt = ""
for y in range(outPutHeight):
    for x in range(outPutWidth):
        txt += get_chars(*img.getpixel((x, y)))
    txt += '\n'

print(txt)
