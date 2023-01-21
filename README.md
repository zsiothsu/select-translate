# select-translate

This software can translate a selected text using google translate api.

## Before installation

`xsel` and `libcurl` is required.

Archlinux:
```shell
pacman -S xsel libcurl-gnutls
```

Ubuntu:
```shell
apt install xsel libcurl4-gnutls-dev
```

## Usage

```
usage:
    select-translate [options]
options:
    -c             --clear             clean screen before translation
    -d <time/s>    --delay             the time between two translation (default 3)
    -s <lang>      --source-language   source language. (default auto)
                                       [Chinese/English/Japanese/Chinese_Simplified
                                        /Chinese_traditional/auto]
    -t <lang>      --target-language   target language. (default Chinese)
                                       [Chinese/English/Japanese/Chinese_Simplified
                                        /Chinese_traditional]
    -h             --help              Display available options.
```

example:

```shell
$ select-translate -c

[source]:１
こんにちは
[translated]
你好

```

