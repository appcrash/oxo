![C/C++ CI](https://github.com/appcrash/oxo/workflows/C/C++%20CI/badge.svg)


# What is OXO

It is mainly a framework used to facilitate building new proxy and protocol. Protocol designer can focus on developing new protocol without messing around with things like event loop, exception handling etc. '''OXO''' is developed in C with small footprint and portability in mind. To this end the whole project is developed under C99 which should be accept by most compilers and the only dependency is libc. So it should be able to run on general OS like linux/freebsd/drawin as well as embedded system like router in your home.


# Can I use OXO in Java/Python or any other language

Yes. '''OXO''' shall export API in shared library and provide binding for more high level language. For example, JNI for Java, ctypes for Python. Most modern languages are capable utilise C shared library easily: golang, rust etc. You can use your favorite language to develop new proxy/protocol if portability and footprint are not the main concerns.

# What is the roadmap

Currently '''OXO''' is under active development and add features, shaping APIs. The roadmap will be updated when alpha version is ready.
