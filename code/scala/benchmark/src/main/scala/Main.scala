import java.nio.file.{Files, Paths}
import java.nio.{ByteBuffer, ByteOrder}
import scala.collection.concurrent.TrieMap

object Constants {
    val NUM_OF_THREADS = 4
}

class Op(val key:Int, val value:Int) {
    def print() {
        println("op " + key + " - " + value)
    }
}

class WorkerThread(m: TrieMap[Int, Int], ops: Array[Op]) extends Runnable {
    def run() {
        for (op <- ops) {
            m(op.key) = op.value
        }
    }
}


object Main {
    def main(args: Array[String]) = {
        if (args.length == 0) {
            println("Please enter a sample path")
        }
        else {
            val map = new TrieMap[Int, Int]

                // Read the data and wrap it with a little endian buffer
                println("Reading data from" + args(0))
                val data = Files.readAllBytes(Paths.get(args(0)))
                val wrapper = ByteBuffer.wrap(data)
                wrapper.order(ByteOrder.LITTLE_ENDIAN);

            val size = wrapper.getInt
                val chunk = size / Constants.NUM_OF_THREADS
                val index = 0

                for (i <- 1 to Constants.NUM_OF_THREADS) {
                    val arr = new Array[Op](chunk)
                        for (j <- 0 to chunk - 1) {
                            val key = wrapper.getInt
                                val value = wrapper.getInt
                                arr(j) = new Op(key, value)
                        }
                    new Thread(new WorkerThread(map, arr)).start()
                }
        }
    }
}
