<!-- Self-test input of check_docs.cmake MODE=links. Fails with 3 failures:
     "broken.md:6: broken link: missing.md",
     "broken.md:7: broken link: sub/missing.png" and
     "broken.md:7: broken link: nowhere/#section". -->
# Broken links
See [missing](missing.md) and [target](target.md).
![image](sub/missing.png) [dir](nowhere/#section)
