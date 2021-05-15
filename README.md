# dbj_json
## DBJ JSON Research and Development

Basically an amalgamation of [GASON](https://github.com/vivkin/gason) and [JSMN](https://github.com/zserge/jsmn)

GASON is transformed into C11 code, and almost rewritten in that process. Also [fast_double_parser](https://github.com/lemire/fast_double_parser) is transformed into C11 code ans used in there.

### What's the idea?

Basically JSMN design is better. But it has no "classical" JSON value abstraction. Will try and make DBJSON that will use JSMN for parsing and GASON for proivding JSON value abstraction. Probably by writting JSMN "token" to GASON "value" transformation.

But, if users are comfortable with dealing JSMN "token" direct that will be provided too.

#### JSON "writer"?

Probably not. We shall see. 

## [valstat](https://valstat.github.io/home/)

Very likely. Although that makes for very safe and not the fastest code. Thus "valstat enabled" front will be provided.

Stay tuned :wink:

---
This work &copy; 2021 by [dbj@dbj.org](https://profile.codersrank.io/user/dbjdbj) aka [Dusan B.Jovanovic](https://dusanjovanovic.org/)
