compilation instructions:

- to compile the file type "make" followed by return key.
- to remove executable files type "make clean" followed by return key.

instructions on running the executable files:

- diskinfo
    - ./diskinfo diskImageName.IMA

- disklist
    - ./disklist diskImageName.IMA

- diskget
    - ./diskget diskImageName.IMA filename.ext

- diskput
    - put into root folder
        - ./diskput diskImageName.IMA filename.ext
        
    - put into a subdirectory
        - ./diskput diskImageName.IMA /subdirname/filename.ext
    - note that the filename and extension must be typed as it appears in your current directory (case sensitive)