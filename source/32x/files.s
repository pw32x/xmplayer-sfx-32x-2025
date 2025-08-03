        .text

        .align  2
fileName1:
        .asciz  "rain.raw"
fileName2:
        .asciz  "punch.raw"
fileName3:
        .asciz  "oof.raw"

        .align  4
file1:
        .incbin "sounds/rain.raw"
fileEnd1:

        .align  4
file2:
        .incbin "sounds/punch.raw"
fileEnd2:

        .align  4
file3:
        .incbin "sounds/oof.raw"
fileEnd3:

        .align  4

        .global _fileName
_fileName:
        .long   fileName1
        .long   fileName2
        .long   fileName3

        .global _fileSize
_fileSize:
        .long   fileEnd1 - file1
        .long   fileEnd2 - file2
        .long   fileEnd3 - file3

        .global _filePtr
_filePtr:
        .long   file1
        .long   file2
        .long   file3

        .align  4
