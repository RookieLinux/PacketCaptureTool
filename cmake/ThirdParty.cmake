include(AddGitDependency)

# -------------------------
# FluentUI
# -------------------------

add_git_dependency(
    FluentUI
    GIT_REPOSITORY https://github.com/zhuzichu520/FluentUI.git
    GIT_TAG main
)

# -------------------------
# PcapPlusPlus
# -------------------------

add_git_dependency(
    PcapPlusPlus
    GIT_REPOSITORY https://github.com/seladb/PcapPlusPlus.git
    GIT_TAG v25.05
)