using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.IO;
using Assimp.Configs;

namespace ModelConverter
{
    public partial class ModelConverter : Form
    {
        //ctor
        public ModelConverter()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterScreen;
        }


        //callbacks
        private void buttonOpen_Click(object sender, EventArgs e)
        {
            OpenFileDialog od = new OpenFileDialog();
            od.Filter = "Obj Files|*.obj";
            if(od.ShowDialog() == DialogResult.OK && loadFile(od.FileName))
            {
                textBoxPath.Text = od.FileName;
            }
        }

        private void buttonSave_Click(object sender, EventArgs e)
        {
            SaveFileDialog sd = new SaveFileDialog();
            sd.Filter = "Cro Model File|*.cmf";
            if (sd.ShowDialog() == DialogResult.OK)
            {
                if (File.Exists(sd.FileName))
                {
                    //warn overwriting
                    if (MessageBox.Show("File Exists. Overwrite?", "Warning", MessageBoxButtons.OKCancel) == DialogResult.Cancel)
                    {
                        return;
                    }
                }
                outputFile(sd.FileName);
            }
            //outputFile("");
        }


        //funcs
        private Assimp.Scene m_scene;
        private bool loadFile(string path)
        {
            Assimp.AssimpContext importer = new Assimp.AssimpContext();
            importer.SetConfig(new NormalSmoothingAngleConfig(66f));
            //TODO check which other post processes we need
            m_scene = importer.ImportFile(path, Assimp.PostProcessSteps.CalculateTangentSpace);

            //failed loading :(
            if (!m_scene.HasMeshes)
            {
                showGroupBoxMessage("No Valid Meshes found.");
                return false;
            }

            //display some info
            string msg = "Mesh Count: " + m_scene.MeshCount.ToString();

            showGroupBoxMessage(msg);

            return true;
        }

        private void outputFile(string name)
        {
            labelInfo.Text = "Processing...";

            if (m_scene.MeshCount == 1)
            {
                //write with normal name
                processOutput(name, m_scene.Meshes[0]);
            }
            else
            {
                //append each file with idx
                for(var i = 0; i < m_scene.MeshCount; ++i)
                {
                    string output = name.Replace(".cmf", i.ToString() + ".cmf");
                    processOutput(output, m_scene.Meshes[i]);
                }
            }

            labelInfo.Text += "\n\nDone!";
        }

        private void processOutput(string output, Assimp.Mesh mesh)
        {
            const int Position = 0;
            const int Colour = 1;
            const int Normal = 2;
            const int Tangent = 3;
            const int Bitangent = 4;
            const int UV0 = 5;
            const int UV1 = 6;
            byte flags = (1 << Position) | (1 << Normal);

            //append any messages to info label
            if (!mesh.HasVertices)
            {
                labelInfo.Text += "\nNo vertex positions found. File not written.";
                return;
            }
            if(mesh.HasVertexColors(0))
            {
                flags |= (1 << Colour);
            }

            if (!mesh.HasNormals)
            {
                labelInfo.Text += "\nNo normals found. File not written.";
                return;
            }
            if(!mesh.HasTangentBasis)
            {
                labelInfo.Text += "\nMesh tangents were missing...";
            }
            else
            {
                flags |= (1 << Tangent) | (1 << Bitangent);
            }

            if (!mesh.HasTextureCoords(0))
            {
                labelInfo.Text += "\nPrimary texture coords are missing, textures will appear undefined.";
            }
            else
            {
                flags |= (1 << UV0);
            }
            if (!mesh.HasTextureCoords(1))
            {
                labelInfo.Text += "\nSecondary texture coords are missing,\n\tlightmapping will be unavailable for this mesh.";
            }
            else
            {
                flags |= (1 << UV1);
            }
            //labelInfo.Text += "\n" + flags.ToString();

            // cmf FILE FORMAT
            /* HEADER
             * flags byte
             * byte index array count (max 32, default 1)
             * <index count> int values representing index array offsets
             * <index count> int values holding index array sizes
             * -------------------------------------------------
             * VBO data interleaved as flags, starts at HEADER size
             * IBO data starts at IBOOFFSET[0] for IBOSIZE[0]
             * All index buffer values are uint32
             * */

            //build VBO data
            List<float> vboData = new List<float>();
            for(var i = 0; i < mesh.VertexCount; ++i)
            {
                var pos = mesh.Vertices[i];
                vboData.Add(pos.X);
                vboData.Add(pos.Y);
                vboData.Add(pos.Z);

                if((flags & (1 << Colour)) != 0)
                {
                    var colour = mesh.VertexColorChannels[0][i];
                    vboData.Add(colour.R);
                    vboData.Add(colour.G);
                    vboData.Add(colour.B);
                }

                var normal = mesh.Normals[i];
                vboData.Add(normal.X);
                vboData.Add(normal.Y);
                vboData.Add(normal.Z);

                if((flags & (1 << Tangent))  != 0)
                {
                    var tan = mesh.Tangents[i];
                    var bitan = mesh.BiTangents[i];
                    vboData.Add(tan.X);
                    vboData.Add(tan.Y);
                    vboData.Add(tan.Z);

                    vboData.Add(bitan.X);
                    vboData.Add(bitan.Y);
                    vboData.Add(bitan.Z);
                }

                if((flags & (1 << UV0)) != 0)
                {
                    var uv = mesh.TextureCoordinateChannels[0][i];
                    vboData.Add(uv.X);
                    vboData.Add(uv.Y);
                }

                if((flags & (1 << UV1)) != 0)
                {
                    var uv = mesh.TextureCoordinateChannels[1][i];
                    vboData.Add(uv.X);
                    vboData.Add(uv.Y);
                }
            }

            var indexArray = mesh.GetUnsignedIndices();

            byte arrayCount = 1;
            int arrayOffset = (sizeof(int) * 2) + 2; //(array offset + array size) + array count + flags
            arrayOffset += (vboData.Count * sizeof(float));
            int arraySize = indexArray.Length * sizeof(uint);

            using (BinaryWriter writer = new BinaryWriter(File.Open(output, FileMode.Create)))
            {
                writer.Write(flags);
                writer.Write(arrayCount);
                writer.Write(arrayOffset);
                writer.Write(arraySize);
                foreach(var v in vboData)
                {
                    writer.Write(v);
                }
                foreach(var i in indexArray)
                {
                    writer.Write(i);
                }
            }
        }

        private void showGroupBoxMessage(string msg)
        {
            //groupBoxInfo.Content.
            //Label l = new Label();
            //l.Text = msg;
            //groupBoxInfo.Controls.Add(l);
            labelInfo.Text = msg;
        }
    }
}
