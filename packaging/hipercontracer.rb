class Hipercontracer < Formula
  desc "High-Performance Connectivity Tracer measurement framework"
  homepage "https://www.nntb.no/~dreibh/hipercontracer/"
  url "https://www.nntb.no/~dreibh/hipercontracer/download/hipercontracer-2.2.10.tar.xz"
  sha256 "f38380da91b78ed350de99afc73cfd42344c99dbafe5d827ebc15b115e3ead03"
  license "GPL-3.0-or-later"

  # Component options without direct third-party dependencies
  option "without-collector", "Build without Collector Tools"
  option "without-dbeaver-tools", "Build without DBeaver Tools"
  option "without-dbshell", "Build without DBShell"
  option "without-example-results", "Include example results"
  option "without-example-scripts", "Include example scripts"
  option "without-icons", "Build without icon and logo files"
  option "without-importer", "Build without HiPerConTracer Importer Tool"
  option "without-node", "Build without HiPerConTracer Node Tools"
  option "without-pipe-checksum", "Build without Pipe Checksum Tool"
  option "without-query", "Build without HiPerConTracer Query Tool"
  option "without-results", "Build without HiPerConTracer Results Tool"
  option "without-rtunnel", "Build without Reverse Tunnel Tool"
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

  # Recommended dependencies (Homebrew automatically generates --without-<dep> flags)
  depends_on "ghostscript" => :recommended
  depends_on "graphicsmagick" => :recommended
  depends_on "libpqxx" => :recommended
  depends_on "mariadb-connector-c" => :recommended
  depends_on "mongo-c-driver" => :recommended
  depends_on "mupdf" => :recommended
  depends_on "rsync" => :recommended

  def install
    build_icons = build.with?("icons") &&
                  build.with?("ghostscript") &&
                  build.with?("graphicsmagick") &&
                  build.with?("mupdf")

    args = std_cmake_args + %W[
      -GNinja
      -DWITH_STATIC_LIBRARIES=OFF
      -DWITH_SHARED_LIBRARIES=ON
      -DSTATIC_BUILD=OFF
      -DWITH_HIPERCONTRACER=ON
      -DWITH_COLLECTOR=#{build.with?("collector") ? "ON" : "OFF"}
      -DWITH_DBEAVER_TOOLS=#{build.with?("dbeaver-tools") ? "ON" : "OFF"}
      -DWITH_DBSHELL=#{build.with?("dbshell") ? "ON" : "OFF"}
      -DWITH_EXAMPLE_RESULTS=#{build.with?("example-results") ? "ON" : "OFF"}
      -DWITH_EXAMPLE_SCRIPTS=#{build.with?("example-scripts") ? "ON" : "OFF"}
      -DWITH_IMPORTER=#{build.with?("importer") ? "ON" : "OFF"}
      -DWITH_NODE=#{build.with?("node") ? "ON" : "OFF"}
      -DWITH_PIPE_CHECKSUM=#{build.with?("pipe-checksum") ? "ON" : "OFF"}
      -DWITH_QUERY=#{build.with?("query") ? "ON" : "OFF"}
      -DWITH_RESULTS=#{build.with?("results") ? "ON" : "OFF"}
      -DWITH_RTUNNEL=#{build.with?("rtunnel") ? "ON" : "OFF"}
      -DWITH_SYNC=#{build.with?("rsync") ? "ON" : "OFF"}
      -DWITH_TRIGGER=#{build.with?("trigger") ? "ON" : "OFF"}
      -DWITH_UDP_ECHO_SERVER=#{build.with?("udp-echo-server") ? "ON" : "OFF"}
      -DWITH_VIEWER=#{build.with?("viewer") ? "ON" : "OFF"}
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
