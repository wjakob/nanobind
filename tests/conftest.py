def pytest_addoption(parser):
    parser.addoption('--enable-slow-tests',
                     action='store_true',
                     dest="enable-slow-tests",
                     default=False,
                     help="enable longrundecorated tests")
