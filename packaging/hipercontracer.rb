class Hipercontracer < Formula
  desc "High-Performance Connectivity Tracer measurement framework"
  homepage "https://www.nntb.no/~dreibh/hipercontracer/"
  url "https://www.nntb.no/~dreibh/hipercontracer/download/hipercontracer-2.2.11.tar.xz"
  sha256 "6668949a5d27284c2d813eb10a5df0ff1642f89c7f8b65376c48d886e57721e0"
  license "GPL-3.0-or-later"

  # Feature options (ON by default, matching FreeBSD OPTIONS_DEFAULT)
  option "without-collector", "Build without Collector Tools"
  option "without-dbeaver-tools", "Build without DBeaver Tools"
  option "without-dbshell", "Build without DBShell"
  option "without-example-results", "Include example results"
  option "without-example-scripts", "Include example scripts"
  option "without-hipercontracer", "Build without HiPerConTracer CLI"
  option "without-icons", "Build without icon and logo files"
  option "without-importer", "Build without HiPerConTracer Importer Tool"
  option "without-libhipercontracer", "Build without HiPerConTracer Library"
  option "without-libhpctdb", "Build without Database Backend Library"
  option "without-libhpctio", "Build without I/O Library"
  option "without-libuniversalimporter", "Build without Universal Importer Library"
  option "without-mariadb", "Build without MariaDB/MySQL database backend"
  option "without-mongodb", "Build without MongoDB database backend"
  option "without-node", "Build without HiPerConTracer Node Tools"
  option "without-pipe-checksum", "Build without Pipe Checksum Tool"
  option "without-postgresql", "Build without PostgreSQL database backend"
  option "without-query", "Build without HiPerConTracer Query Tool"
  option "without-results", "Build without HiPerConTracer Results Tool"
  option "without-rtunnel", "Build without Reverse Tunnel Tool"
  option "without-sync", "Build without Synchronisation Tool"
  option "without-trigger", "Build without HiPerConTracer Trigger Tool"
  option "without-udp-echo-server", "Build without UDP Echo Server"
  option "without-viewer", "Build without Viewer Tool"

  # Build-time dependencies
  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build

  # Mandatory runtime dependencies
  depends_on "boost"
  depends_on "bzip2"
  depends_on "openssl@3"
  depends_on "xz"
  depends_on "zstd"

  # Feature-driven dependencies
  depends_on "ghostscript" if build.with? "icons"
  depends_on "graphicsmagick" if build.with? "icons"
  depends_on "pdf2svg" if build.with? "icons"

  depends_on "mariadb-connector-c" if build.with? "mariadb"
  depends_on "mongo-c-driver" if build.with? "mongodb"
  depends_on "libpqxx" if build.with? "postgresql"

  depends_on "rsync" if build.with? "sync"

  def install
    args = std_cmake_args + %W[
      -GNinja
      -DCMAKE_INSTALL_RPATH=#{rpath}
      -DWITH_STATIC_LIBRARIES=OFF
      -DWITH_SHARED_LIBRARIES=ON
      -DSTATIC_BUILD=OFF
      -DWITH_COLLECTOR=#{build.with?("collector") ? "ON" : "OFF"}
      -DWITH_DBEAVER_TOOLS=#{build.with?("dbeaver-tools") ? "ON" : "OFF"}
      -DWITH_DBSHELL=#{build.with?("dbshell") ? "ON" : "OFF"}
      -DWITH_EXAMPLE_RESULTS=#{build.with?("example-results") ? "ON" : "OFF"}
      -DWITH_EXAMPLE_SCRIPTS=#{build.with?("example-scripts") ? "ON" : "OFF"}
      -DWITH_HIPERCONTRACER=#{build.with?("hipercontracer") ? "ON" : "OFF"}
      -DWITH_ICONS=#{build.with?("icons") ? "ON" : "OFF"}
      -DWITH_IMPORTER=#{build.with?("importer") ? "ON" : "OFF"}
      -DWITH_LIBHIPERCONTRACER=#{build.with?("libhipercontracer") ? "ON" : "OFF"}
      -DWITH_LIBHPCTDB=#{build.with?("libhpctdb") ? "ON" : "OFF"}
      -DWITH_LIBHPCTIO=#{build.with?("libhpctio") ? "ON" : "OFF"}
      -DWITH_LIBUNIVERSALIMPORTER=#{build.with?("libuniversalimporter") ? "ON" : "OFF"}
      -DWITH_NODE=#{build.with?("node") ? "ON" : "OFF"}
      -DWITH_PIPE_CHECKSUM=#{build.with?("pipe-checksum") ? "ON" : "OFF"}
      -DWITH_QUERY=#{build.with?("query") ? "ON" : "OFF"}
      -DWITH_RESULTS=#{build.with?("results") ? "ON" : "OFF"}
      -DWITH_RTUNNEL=#{build.with?("rtunnel") ? "ON" : "OFF"}
      -DWITH_SYNC=#{build.with?("sync") ? "ON" : "OFF"}
      -DWITH_TRIGGER=#{build.with?("trigger") ? "ON" : "OFF"}
      -DWITH_UDP_ECHO_SERVER=#{build.with?("udp-echo-server") ? "ON" : "OFF"}
      -DWITH_VIEWER=#{build.with?("viewer") ? "ON" : "OFF"}
      -DENABLE_BACKEND_DEBUG=ON
      -DENABLE_BACKEND_MARIADB=#{build.with?("mariadb") ? "ON" : "OFF"}
      -DENABLE_BACKEND_POSTGRESQL=#{build.with?("postgresql") ? "ON" : "OFF"}
      -DENABLE_BACKEND_MONGODB=#{build.with?("mongodb") ? "ON" : "OFF"}
    ]

    system "cmake", "-S", ".", "-B", "build", *args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/hipercontracer --version 2>&1")
  end
end
