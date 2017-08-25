import java.nio.file.{Files, Paths}
import java.nio.{ByteBuffer, ByteOrder}
import scala.collection.concurrent.TrieMap
import scala.collection.mutable.ListBuffer

class MyInt (val x: Int){
    override def hashCode() = {x/10}
    override def equals(o: Any) = o match {
        case that : MyInt => (x == that.x)
        case _ => false
    }
    override def toString() = x.toString
}

object Constants {
    val NUM_OF_THREADS = 44
}

trait Op[Key, Value] {
    def execute(m: TrieMap[Key, Value])
}

class InsertOp[Key, Value] (key:Key, value:Value) extends Op[Key, Value]{
    override def execute(m: TrieMap[Key, Value]) {
        //println("inserting " +key.toString +":"+value)
        m(key) = value
    }
}

class LookupOp[Key, Value] (key:Key) extends Op[Key, Value] {
    override def execute(m: TrieMap[Key, Value]) {
        try {
            val value = m(key)
            //println("got " + value + " for " + key)
        } 
        catch {
        case e : Exception => println("got exception for " + key)
        }
    }
}

class RemoveOp[Key, Value] (key:Key) extends Op[Key, Value] {
    override def execute(m: TrieMap[Key, Value]) {
        try {
            m.remove(key)
            //println("removed " + key)
        } 
        catch {
        case e : Exception => println("got exception for " + key)
        }
    }
}

class WorkerThread[Key, Value](m: TrieMap[Key, Value], ops: Array[Op[Key, Value]]) extends Runnable {
    def run() {
        for (op <- ops) {
            op.execute(m)
        }
    }
}


object Main {
    def handleInsert (map: TrieMap[MyInt, Int], path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);

        val size = wrapper.getInt
        val chunk = size / Constants.NUM_OF_THREADS
        val index = 0

        val threads = new ListBuffer[Thread]()

        for (i <- 1 to Constants.NUM_OF_THREADS) {
            val arr = new Array[Op[MyInt, Int]](chunk)
            for (j <- 0 to chunk - 1) {
                val key = new MyInt(wrapper.getInt)
                val value = wrapper.getInt
                arr(j) = new InsertOp[MyInt, Int](key, value)
            }
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, arr))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done insert in " + (end_time - start_time) + " nsec");
    }

    def handleLookup (map: TrieMap[MyInt, Int], path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);

        val size = wrapper.getInt
        val chunk = size / Constants.NUM_OF_THREADS
        val index = 0

        val threads = new ListBuffer[Thread]()

        for (i <- 1 to Constants.NUM_OF_THREADS) {
            val arr = new Array[Op[MyInt, Int]](chunk)
            for (j <- 0 to chunk - 1) {
                val key = new MyInt(wrapper.getInt)
                arr(j) = new LookupOp[MyInt, Int](key)
            }
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, arr))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done lookup in " + (end_time - start_time) + " nsec");
    }

    def handleRemove (map: TrieMap[MyInt, Int], path: String) = {
        // Read the data and wrap it with a little endian buffer
        println("Reading data from" + path)
        val data = Files.readAllBytes(Paths.get(path))
        val wrapper = ByteBuffer.wrap(data)
        wrapper.order(ByteOrder.LITTLE_ENDIAN);

        val size = wrapper.getInt
        val chunk = size / Constants.NUM_OF_THREADS
        val index = 0

        val threads = new ListBuffer[Thread]()

        for (i <- 1 to Constants.NUM_OF_THREADS) {
            val arr = new Array[Op[MyInt, Int]](chunk)
            for (j <- 0 to chunk - 1) {
                val key = new MyInt(wrapper.getInt)
                arr(j) = new RemoveOp[MyInt, Int](key)
            }
            threads +=  new Thread(new WorkerThread[MyInt, Int](map, arr))
        }
        val start_time = System.nanoTime()
        threads.foreach(t => t.start())
        threads.foreach(t => t.join())
        val end_time = System.nanoTime()
        println("Done remove in " + (end_time - start_time) + " nsec");
    }

    def main(args: Array[String]) = {
        if (args.length == 0) {
            println("Please enter a sample path")
        }
        else {
            val map = new TrieMap[MyInt, Int]()

            for (i <- 0 to args.length - 1 by 2) {
                if (args(i) == "insert") {
                    handleInsert(map, args(i+1))
                }
                if (args(i) == "lookup") {
                    handleLookup(map, args(i+1))
                }
                if (args(i) == "remove") {
                    handleRemove(map, args(i+1))
                }
            }
        }
    }
}
