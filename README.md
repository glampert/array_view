
## array_view

Implementation of `array_view<T>` and `strided_array_view<T>`,
inspired by [`span<T>`](https://github.com/Microsoft/GSL/blob/master/include/span.h)
from GSL (AKA the [C++ Core Guidelines](https://github.com/Microsoft/GSL)).

Also see [this Standard proposal for `array_view`](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3851.pdf).

This implementation is not fully compatible with the above proposal or `span<T>`,
it merely borrows the name and the concept of wrapping a pointer and a size into a template class.
I have only implemented enough functionality to suit my needs. This wasn't very thoroughly tested
either, so it is likely to have bugs or unexpected corner cases that are not handled well.

### License

This software is in the *public domain*. Where that dedication is not recognized,
you are granted a perpetual, irrevocable license to copy, distribute, and modify
the code as you see fit.

Source code is provided "as is", without warranty of any kind, express or implied.
No attribution is required, but a mention about the author is appreciated.

