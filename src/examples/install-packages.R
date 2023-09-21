dir.create(path = Sys.getenv("R_LIBS_USER"), showWarnings = FALSE, recursive = TRUE)
options(repos = "https://cran.uib.no/", Ncpus = parallel::detectCores(), lib = Sys.getenv("R_LIBS_USER"))

update.packages(ask=FALSE, lib = Sys.getenv("R_LIBS_USER"))

install.packages("assert", lib = Sys.getenv("R_LIBS_USER"))
install.packages("anytime", lib = Sys.getenv("R_LIBS_USER"))
install.packages("digest", lib = Sys.getenv("R_LIBS_USER"))
install.packages("data.table", lib = Sys.getenv("R_LIBS_USER"))
install.packages("dplyr", lib = Sys.getenv("R_LIBS_USER"))
install.packages("ipaddress", lib = Sys.getenv("R_LIBS_USER"))
