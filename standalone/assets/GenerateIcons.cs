using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

// Editable vector source for the standalone utility's icon. No external tools
// or downloaded artwork: rerun standalone/build.ps1 to rebuild every size.
internal static class GenerateIcons
{
    private static GraphicsPath RoundedRect(RectangleF rect, float radius)
    {
        GraphicsPath path = new GraphicsPath();
        float d = radius * 2;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }

    private static Bitmap Draw(int size, Color accent)
    {
        using (Bitmap large = new Bitmap(size * 4, size * 4, PixelFormat.Format32bppArgb))
        {
            using (Graphics g = Graphics.FromImage(large))
            {
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.ScaleTransform(large.Width / 256f, large.Height / 256f);
                using (GraphicsPath tile = RoundedRect(new RectangleF(8, 8, 240, 240), 51))
                using (LinearGradientBrush background = new LinearGradientBrush(new Point(0, 0), new Point(0, 256), Color.FromArgb(37, 57, 69), Color.FromArgb(14, 26, 35)))
                using (Pen border = new Pen(Color.FromArgb(78, 104, 119), 3))
                {
                    g.FillPath(background, tile);
                    g.DrawPath(border, tile);
                }
                using (Pen rim = new Pen(Color.FromArgb(238, 246, 249), 14))
                using (Pen ticks = new Pen(Color.FromArgb(174, 194, 204), 5))
                using (SolidBrush pointer = new SolidBrush(accent))
                {
                    rim.StartCap = rim.EndCap = LineCap.Round;
                    ticks.StartCap = ticks.EndCap = LineCap.Round;
                    g.DrawArc(rim, 48, 43, 160, 160, 135, 270);
                    for (int i = 0; i <= 6; i++)
                    {
                        double angle = (135 + i * 45) * Math.PI / 180;
                        g.DrawLine(ticks, 128 + (float)Math.Cos(angle) * 62, 123 + (float)Math.Sin(angle) * 62,
                            128 + (float)Math.Cos(angle) * 70, 123 + (float)Math.Sin(angle) * 70);
                    }
                    g.FillPolygon(pointer, new PointF[] { new PointF(117, 122), new PointF(176, 75), new PointF(134, 135) });
                    g.FillEllipse(pointer, 116, 111, 24, 24);
                }
                using (Pen snow = new Pen(accent, 5))
                {
                    snow.StartCap = snow.EndCap = LineCap.Round;
                    for (int i = 0; i < 6; i++)
                    {
                        double a = (i * 60 - 90) * Math.PI / 180;
                        PointF tip = new PointF(128 + (float)Math.Cos(a) * 23, 201 + (float)Math.Sin(a) * 23);
                        PointF fork = new PointF(128 + (float)Math.Cos(a) * 15, 201 + (float)Math.Sin(a) * 15);
                        g.DrawLine(snow, 128, 201, tip.X, tip.Y);
                        foreach (int direction in new int[] { -1, 1 })
                        {
                            double branch = a + direction * Math.PI / 3;
                            g.DrawLine(snow, fork, new PointF(fork.X + (float)Math.Cos(branch) * 9, fork.Y + (float)Math.Sin(branch) * 9));
                        }
                    }
                }
            }
            Bitmap small = new Bitmap(size, size, PixelFormat.Format32bppArgb);
            using (Graphics g = Graphics.FromImage(small))
            {
                g.CompositingMode = CompositingMode.SourceCopy;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.DrawImage(large, new Rectangle(0, 0, size, size));
            }
            return small;
        }
    }

    private static void Save(string directory, string name, Color accent)
    {
        int[] sizes = { 16, 20, 24, 32, 40, 48, 64, 128, 256 };
        List<byte[]> frames = new List<byte[]>();
        foreach (int size in sizes)
        {
            using (Bitmap bitmap = Draw(size, accent))
            using (MemoryStream png = new MemoryStream())
            {
                bitmap.Save(png, ImageFormat.Png);
                frames.Add(png.ToArray());
                if (size == 256) bitmap.Save(Path.Combine(directory, name + ".png"), ImageFormat.Png);
            }
        }
        using (BinaryWriter ico = new BinaryWriter(File.Create(Path.Combine(directory, name + ".ico"))))
        {
            ico.Write((ushort)0); ico.Write((ushort)1); ico.Write((ushort)sizes.Length);
            int offset = 6 + 16 * sizes.Length;
            for (int i = 0; i < sizes.Length; i++)
            {
                ico.Write((byte)(sizes[i] == 256 ? 0 : sizes[i]));
                ico.Write((byte)(sizes[i] == 256 ? 0 : sizes[i]));
                ico.Write((byte)0); ico.Write((byte)0); ico.Write((ushort)1); ico.Write((ushort)32);
                ico.Write(frames[i].Length); ico.Write(offset);
                offset += frames[i].Length;
            }
            foreach (byte[] frame in frames) ico.Write(frame);
        }
    }

    private static void Main(string[] args)
    {
        string directory = args.Length == 1 ? args[0] : AppDomain.CurrentDomain.BaseDirectory;
        Directory.CreateDirectory(directory);
        Save(directory, "cruise-control", Color.FromArgb(255, 191, 70));
        Save(directory, "cruise-control-on", Color.FromArgb(97, 226, 151));
        Save(directory, "cruise-control-error", Color.FromArgb(255, 108, 99));
        Console.WriteLine("Built application and tray icons: 9 sizes, 3 states.");
    }
}
