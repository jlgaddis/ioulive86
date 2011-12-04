all:
	gcc -o ioulive86 ioulive.c iou-raw.c parse.c
clean:
	rm -rf ioulive86
