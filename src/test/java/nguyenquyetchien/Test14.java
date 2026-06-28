package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import java.util.stream.Stream;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test14 {

    // Hàm cung cấp dữ liệu mẫu cho bài test mảng
    static Stream<Object[]> arrayProvider() {
        return Stream.of(
            new Object[]{new int[]{1, 2, 3, 4, 5}, 15},     // Mảng thông thường: 1+2+3+4+5 = 15
            new Object[]{new int[]{-1, -2, 3, 4}, 4},       // Mảng có cả số âm và dương
            new Object[]{new int[]{}, 0},                   // Mảng rỗng (vòng lặp không chạy) -> trả về 0
            new Object[]{new int[]{7}, 7},                  // Mảng chỉ có 1 phần tử
            new Object[]{new int[]{100, -100, 50}, 50}      // Mảng triệt tiêu nhau
        );
    }

    @ParameterizedTest(name = "Test tính tổng mảng case {index}")
    @MethodSource("arrayProvider")
    void testCalculateSum(int[] arr, int expectedSum) {
        assertEquals(expectedSum, ArraySum.calculateSum(arr));
    }
}