library("data.table")
library("ggplot2")
library(gtable)

data <- fread("results.data")


cairo_pdf("compressor-tests.pdf", width=15, height=7.5, family="Helvetica", pointsize=24, onefile = TRUE)


p1 <- ggplot(data, aes(x = Level, y = Size / 1024**2, color = Compression)) +
  geom_line(linewidth = 2.5) +
  facet_grid(cols = vars(Input)) +
  scale_color_brewer(palette="Set1") +
  scale_x_continuous("Compression Level Setting", breaks = seq(0, 19, 1)) +
  scale_y_continuous("Compressed File Size [MiB]") +
  labs(title = "Resulting File Size")

print(p1)


p2 <- ggplot(data, aes(x = Level, y = Real, color = Compression)) +
  geom_line(linewidth = 2.5) +
  facet_grid(cols = vars(Input)) +
  scale_color_brewer(palette="Set1") +
  scale_x_continuous("Compression Level Setting", breaks = seq(0, 19, 1)) +
  scale_y_continuous("Run-Time [s]") +
  labs(title = "Real Run-Time")

print(p2)


dev.off()
