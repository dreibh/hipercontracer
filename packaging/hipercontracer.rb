class Hipercontracer < Formula
  desc "High-Performance Connectivity Tracer measurement framework"
  homepage "https://www.nntb.no/~dreibh/hipercontracer/"
  url "https://www.nntb.no/~dreibh/hipercontracer/download/hipercontracer-2.2.10.tar.xz"
  sha256 "f38380da91b78ed350de99afc73cfd42344c99dbafe5d827ebc15b115e3ead03"
  license "GPL-3.0-or-later"

  option "with-icons", "Build icon and logo files"
  option "without-collector", "Build without Collector Tools"
  option "without-importer", "Build without HiPerConTracer Importer Tool"
  option "without-sync", "Build without Synchronization Tool"
  option "without-viewer", "Build without Viewer Tool"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  depends_on "boost"
  depends_on "bzip2"
  depends_on "openssl@3"
  depends_on "xz"
  depends_on "zstd"

  depends_on "ghostscript" => :optional
  depends_on "graphicsmagick" => :optional
  depends_on "libpqxx" => :optional
  depends_on "mariadb-connector-c" => :optional
  depends_on "mongo-c-driver" => :optional
  depends_on "mupdf" => :optional
  depends_on "rsync" => :optional

  def install
    build_icons = build.with?("icons") ||
                  (build.with?("ghostscript") &&
                   build.with?("graphicsmagick") &&
                   build.with?("mupdf"))

    args = std_cmake_args + %W[
      -GNinja
      -DWITH_STATIC_LIBRARIES=OFF
      -DWITH_SHARED_LIBRARIES=ON
      -DSTATIC_BUILD=OFF
      -DWITH_HIPERCONTRACER=ON
      -DWITH_TRIGGER=ON
      -DWITH_QUERY=ON
      -DWITH_RESULTS=ON
      -DWITH_RTUNNEL=ON
      -DWITH_NODE=ON
      -DWITH_DBSHELL=ON
      -DWITH_DBEAVER_TOOLS=ON
      -DWITH_PIPE_CHECKSUM=ON
      -DWITH_UDP_ECHO_SERVER=ON
      -DWITH_EXAMPLE_RESULTS=ON
      -DWITH_EXAMPLE_SCRIPTS=ON
      -DWITH_COLLECTOR=#{build.with?("collector") ? "ON" : "OFF"}
      -DWITH_SYNC=#{build.with?("sync") ? "ON" : "OFF"}
      -DWITH_VIEWER=#{build.with?("viewer") ? "ON" : "OFF"}
      -DWITH_IMPORTER=#{build.with?("importer") ? "ON" : "OFF"}
      -DWITH_ICONS=#{build_icons ? "ON" : "OFF"}
      -DENABLE_BACKEND_DEBUG=ON
      -DENABLE_BACKEND_MARIADB=#{build.with?("mariadb-connector-c") ? "ON" : "OFF"}
      -DENABLE_BACKEND_POSTGRESQL=#{build.with?("libpqxx") ? "ON" : "OFF"}
      -DENABLE_BACKEND_MONGODB=#{build.with?("mongo-c-driver") ? "ON" : "OFF"}
    ]

    system "cmake", "-S", ".", "-B", "build", *args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/hipercontracer --version")
  end
end
