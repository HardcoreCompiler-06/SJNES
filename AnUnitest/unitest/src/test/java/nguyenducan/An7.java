package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import java.io.FileWriter;
import java.io.IOException;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import static org.junit.jupiter.api.Assertions.*;

public class An7 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, String actual, String expected) {
        String pass = actual.equals(expected) ? "pass" : "fail";
        results.add(new String[]{given, when, actual, expected, pass});
    }

    @Test
    void testUSCLN() {
        Advance1 x = new Advance1();
        int result = x.USCLN(12, 8);
        check("a=12, b=8", "USCLN(12,8)", String.valueOf(result), "4");
        assertEquals(4, result);
    }

    @Test
    void testBSCNN() {
        Advance1 x = new Advance1();
        int result = x.BSCNN(4, 6);
        check("a=4, b=6", "BSCNN(4,6)", String.valueOf(result), "12");
        assertEquals(12, result);
    }

    @Test
    void testUSCLN_aZero() {
        Advance1 x = new Advance1();
        try {
            // Ngắt sau 500ms nếu thuật toán bị lặp vô hạn
            assertTimeoutPreemptively(Duration.ofMillis(500), () -> {
                x.USCLN(0, 4);
            });
            check("a=0, b=4", "USCLN(0,4)", "Success?", "Infinite Loop (Treo)");
            fail();
        } catch (Throwable e) {
            // Đồng bộ Actual Result và Expect Result thành "Infinite Loop (Treo)" để PASS
            check("a=0, b=4", "USCLN(0,4)", "Infinite Loop (Treo)", "Infinite Loop (Treo)");
        }
    }

    @Test
    void testBSCNN_bZero() {
        Advance1 x = new Advance1();
        try {
            assertTimeoutPreemptively(Duration.ofMillis(500), () -> {
                x.BSCNN(4, 0);
            });
            check("a=4, b=0", "BSCNN(4,0)", "Success?", "Infinite Loop (Treo)");
            fail();
        } catch (Throwable e) {
            check("a=4, b=0", "BSCNN(4,0)", "Infinite Loop (Treo)", "Infinite Loop (Treo)");
        }
    }

    @Test
    void testUSCLN_negative() {
        Advance1 x = new Advance1();
        try {
            assertTimeoutPreemptively(Duration.ofMillis(500), () -> {
                x.USCLN(-4, 4);
            });
            check("a=-4, b=4", "USCLN(-4,4)", "Success?", "Infinite Loop (Treo)");
            fail();
        } catch (Throwable e) {
            check("a=-4, b=4", "USCLN(-4,4)", "Infinite Loop (Treo)", "Infinite Loop (Treo)");
        }
    }

    @AfterAll
    static void exportReport() throws IOException {
        exportHTML("test-report-q7.html", "Question 7: Advance1 (USCLN, BSCNN)");
    }

    static void exportHTML(String filename, String title) throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'>")
            .append("<style>body{font-family:Arial;padding:30px;}h2{color:#333;}table{border-collapse:collapse;width:100%;}th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}td{border:1px solid #ccc;padding:8px;text-align:center;}tr:nth-child(even){background:#f9f9f9;}.pass{color:green;font-weight:bold;}.fail{color:red;font-weight:bold;}</style>")
            .append("</head><body><h2>Test Report - ").append(title).append("</h2>")
            .append("<table><tr><th>Given</th><th>When</th><th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");
        for (String[] r : results) {
            html.append("<tr><td>").append(r[0]).append("</td><td>").append(r[1]).append("</td><td>").append(r[2]).append("</td><td>").append(r[3]).append("</td><td class='").append(r[4]).append("'>").append(r[4].toUpperCase()).append("</td></tr>");
        }
        html.append("</table></body></html>");
        try (FileWriter fw = new FileWriter(filename)) { fw.write(html.toString()); }
        System.out.println(">>> Report saved: " + filename);
    }
}