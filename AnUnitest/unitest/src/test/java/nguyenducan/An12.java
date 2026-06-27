package nguyenducan;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class An12 {

    static List<String[]> results = new ArrayList<>();

    static void check(String given, String when, String actual, String expected) {
        String pass = actual.equals(expected) ? "Pass" : "Fail";
        results.add(new String[]{given, when, actual, expected, pass});
    }

    @Test
    void testTinhTuoi() {
        Advance6 x = new Advance6();
        int result = x.tinhTuoi(12, 1, 1999);
        check("ngay=12, thang=1, nam=1999", "tinhTuoi(12,1,1999)", String.valueOf(result), "27");
        assertTrue(result >= 0);
    }

    @Test
    void testFutureYear() {
        Advance6 x = new Advance6();
        int result = x.tinhTuoi(12, 1, 2030);
        check("ngay=12, thang=1, nam=2030", "tinhTuoi(12,1,2030)", String.valueOf(result), "-1");
        assertEquals(-1, result);
    }

    @Test
    void testInvalidDay() {
        Advance6 x = new Advance6();
        int result = x.tinhTuoi(-12, 1, 2030);
        check("ngay=-12, thang=1, nam=2030", "tinhTuoi(-12,1,2030)", String.valueOf(result), "-1");
        assertEquals(-1, result);
    }

    @Test
    void testInvalidMonth() {
        Advance6 x = new Advance6();
        int result = x.tinhTuoi(12, -1, 2030);
        check("ngay=12, thang=-1, nam=2030", "tinhTuoi(12,-1,2030)", String.valueOf(result), "-1");
        assertEquals(-1, result);
    }

    @Test
    void testInvalidYear() {
        Advance6 x = new Advance6();
        int result = x.tinhTuoi(12, 1, -2030);
        check("ngay=12, thang=1, nam=-2030", "tinhTuoi(12,1,-2030)", String.valueOf(result), "-1");
        assertEquals(-1, result);
    }

    @AfterAll
    static void exportReport() throws IOException {
        StringBuilder html = new StringBuilder();
        html.append("<!DOCTYPE html><html><head><meta charset='UTF-8'><style>body{font-family:Arial;padding:30px;}h2{color:#333;}table{border-collapse:collapse;width:100%;}th{background:#4CAF50;color:white;padding:10px;border:1px solid #ccc;}td{border:1px solid #ccc;padding:8px;text-align:center;}tr:nth-child(even){background:#f9f9f9;}.pass{color:green;font-weight:bold;}.fail{color:red;font-weight:bold;}</style></head><body>")
            .append("<h2>Test Report - Question 12: Advance6 (tinhTuoi)</h2>")
            .append("<table><tr><th>Given</th><th>When</th><th>Actual Result</th><th>Expect Result</th><th>Pass/Fail</th></tr>");
        for (String[] r : results) {
            String css = r[4].equals("Pass") ? "pass" : "fail";
            html.append("<tr><td>").append(r[0]).append("</td><td>").append(r[1]).append("</td><td>").append(r[2]).append("</td><td>").append(r[3]).append("</td><td class='").append(css).append("'>").append(r[4]).append("</td></tr>");
        }
        html.append("</table></body></html>");
        try (FileWriter fw = new FileWriter("test-report-q12.html")) { fw.write(html.toString()); }
        System.out.println(">>> Report saved: test-report-q12.html");
    }
}