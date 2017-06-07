using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.IO;
using Assimp.Configs;

namespace ModelConverter
{
    public partial class ModelConverter : Form
    {
        private const int maxMaterials = 32;
        private string m_currentFileName;
        
        //ctor
        public ModelConverter()
        {
            InitializeComponent();
            this.StartPosition = FormStartPosition.CenterScreen;
            comboBoxShaderType.SelectedIndex = 0;
        }


        //callbacks
        private void buttonOpen_Click(object sender, EventArgs e)
        {
            OpenFileDialog od = new OpenFileDialog();
            od.Filter = "Obj Files|*.obj|Collada Files|*.DAE|FBX Files|*.fbx";
            if(od.ShowDialog() == DialogResult.OK && loadFile(od.FileName))
            {
                textBoxPath.Text = od.FileName;
                m_currentFileName = Path.GetFileNameWithoutExtension(od.FileName);
                this.Text = "Model Converter - " + Path.GetFileName(od.FileName);
            }
        }

        private void buttonSave_Click(object sender, EventArgs e)
        {
            SaveFileDialog sd = new SaveFileDialog();
            sd.Filter = "Cro Model File|*.cmf";
            sd.FileName = m_currentFileName + ".cmf";
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
                outputCMT(sd.FileName);
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
            m_scene = importer.ImportFile(path, 
                Assimp.PostProcessSteps.CalculateTangentSpace 
                | Assimp.PostProcessSteps.GenerateNormals 
                | Assimp.PostProcessSteps.Triangulate
                | Assimp.PostProcessSteps.JoinIdenticalVertices
                | Assimp.PostProcessSteps.OptimizeMeshes);

            //failed loading :(
            if (!m_scene.HasMeshes)
            {
                textBoxInfo.Text = "No Valid Meshes found.";
                return false;
            }

            //display some info
            string msg = "Mesh Count: " + m_scene.MeshCount.ToString()
                + Environment.NewLine + "Material count: " + m_scene.MaterialCount.ToString()
                + " (Maximum material count is 32)";

            textBoxInfo.Text = msg;

            buttonSave.Enabled = (m_scene.MeshCount > 0 && m_scene.MaterialCount <= maxMaterials);

            return true;
        }

        private byte m_currentFlags = 0;
        private void outputFile(string name)
        {
            textBoxInfo.Text = "Processing...";

            /*
             * Apparently assimp creates one or more meshes for each material
             * The meshes should be sorted by material type and joined into 
             * a single VBO. Each time a new material type is encountered the current
             * VBO size should be kept as the material index offset.
             * Then, for each material, an index array created adding the index
             * offset to each value. A material list can be added to the header
             * to correctly map materials to a mesh.
            */

            m_currentFlags = getFlags(m_scene.Meshes[0]);
            if(m_currentFlags == 0)
            {
                textBoxInfo.Text += Environment.NewLine + "Invalid vertex data, file not written.";
                return;
            }

            m_scene.Meshes.Sort((x, y)=> x.MaterialIndex.CompareTo(y.MaterialIndex));
            List<float> vboData = new List<float>();
            List<int>[] indexArrays = new List<int>[m_scene.MaterialCount];
            int indexOffset = 0;
            for(var i = 0; i < m_scene.MeshCount; ++i)
            {
                if (addToVBO(m_scene.Meshes[i], ref vboData)) //only added if flags match first mesh
                {
                    addToIndexArray(m_scene.Meshes[i], ref indexArrays, indexOffset);
                    indexOffset += m_scene.Meshes[i].VertexCount;
                }
            }

            List<int> arrayOffsets = new List<int>();
            List<int> arraySizes = new List<int>();
            int currentOffset = vboData.Count * sizeof(float);
            foreach(var list in indexArrays)
            {
                if(list != null)
                {
                    //add offset
                    arrayOffsets.Add(currentOffset);
                    arraySizes.Add(list.Count * sizeof(int));
                    currentOffset += arraySizes[arraySizes.Count - 1];
                }
            }

            //check array sizes are valid
            if(arrayOffsets.Count != arraySizes.Count)
            {
                textBoxInfo.Text += Environment.NewLine + "Invalid array data sizes, output failed.";
                return;
            }

            byte arrayCount = (byte)arraySizes.Count;
            //add header size to all offsets
            int headerSize = ((sizeof(int) * arrayCount) * 2) + 2; //(array offset + array size) + array count + flags
            for(var i = 0; i < arrayCount; ++i)
            {
                arrayOffsets[i] += headerSize;
            }

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

            //write all to file
            using (BinaryWriter writer = new BinaryWriter(File.Open(name, FileMode.Create)))
            {
                writer.Write(m_currentFlags);
                writer.Write(arrayCount);
                foreach (var v in arrayOffsets)
                {
                    writer.Write(v);
                }
                foreach (var v in arraySizes)
                {
                    writer.Write(v);
                }
                foreach (var v in vboData)
                {
                    writer.Write(v);
                }
                foreach (var indexArray in indexArrays)
                {
                    if (indexArray != null)
                    {
                        foreach (var i in indexArray)
                        {
                            writer.Write(i);
                        }
                    }
                }
            }

            textBoxInfo.Text += Environment.NewLine + "Wrote one mesh with " + arrayCount.ToString() + " submeshes.";
            textBoxInfo.Text += Environment.NewLine + "Done!";
            m_currentFlags = 0;
        }

        private bool addToVBO(Assimp.Mesh mesh, ref List<float> vboData)
        {
            if(getFlags(mesh) != m_currentFlags)
            {
                textBoxInfo.Text += Environment.NewLine + "Skipping mesh with invalid vertex data...";
                return false;
            }
            for (var i = 0; i < mesh.VertexCount; ++i)
            {
                var pos = mesh.Vertices[i];
                vboData.Add(pos.X);
                vboData.Add(pos.Y);
                vboData.Add(pos.Z);

                if (mesh.HasVertexColors(0)) 
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

                if (mesh.HasTangentBasis)
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

                if (mesh.HasTextureCoords(0))
                {
                    var uv = mesh.TextureCoordinateChannels[0][i];
                    vboData.Add(uv.X);
                    vboData.Add(uv.Y);
                }

                if (mesh.HasTextureCoords(1))
                {
                    var uv = mesh.TextureCoordinateChannels[1][i];
                    vboData.Add(uv.X);
                    vboData.Add(uv.Y);
                }
            }
            return true;
        }

        private void addToIndexArray(Assimp.Mesh mesh, ref List<int>[] idxArray, int offset)
        {
            int idx = mesh.MaterialIndex;

            if (idxArray[idx] == null) idxArray[idx] = new List<int>();

            for(var i = 0; i < mesh.FaceCount; ++i)
            {
                idxArray[idx].Add(mesh.Faces[i].Indices[0] + offset);
                idxArray[idx].Add(mesh.Faces[i].Indices[1] + offset);
                idxArray[idx].Add(mesh.Faces[i].Indices[2] + offset);
            }
        }

        private byte getFlags(Assimp.Mesh mesh)
        {
            const int Position = 0; //NOTE this must match the Mesh::Attribute enum of cro
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
                if(m_currentFlags == 0) textBoxInfo.Text += Environment.NewLine + "No vertex positions found. File not written.";
                return 0;
            }
            if (mesh.HasVertexColors(0))
            {
                flags |= (1 << Colour);
                textBoxInfo.Text += Environment.NewLine + "Colour data found in vertices.";
            }

            if (!mesh.HasNormals)
            {
                if (m_currentFlags == 0) textBoxInfo.Text += Environment.NewLine + "No normals found. File not written.";
                return 0;
            }
            if (!mesh.HasTangentBasis)
            {
                if (m_currentFlags == 0) textBoxInfo.Text += Environment.NewLine + "Mesh tangents were missing...";
            }
            else
            {
                flags |= (1 << Tangent) | (1 << Bitangent);
            }

            if (!mesh.HasTextureCoords(0))
            {
                if (m_currentFlags == 0) textBoxInfo.Text += Environment.NewLine + "Primary texture coords are missing, textures will appear undefined.";
            }
            else
            {
                flags |= (1 << UV0);
            }
            if (!mesh.HasTextureCoords(1))
            {
                if (m_currentFlags == 0) textBoxInfo.Text += Environment.NewLine + "Secondary texture coords are missing, lightmapping will be unavailable for this mesh.";
            }
            else
            {
                flags |= (1 << UV1);
            }
            return flags;
        }

        private string colourToString(Assimp.Color4D colour)
        {
            return colour.R.ToString() + "," + colour.G.ToString() + "," + colour.B.ToString() + "," + colour.A.ToString();
        }

        private void outputCMT(string path)
        {
            //writes material / model data to a cmt file
            string outpath = path.Substring(0, path.Length -1);
            outpath += 't';

            try
            {
                string modelName = Path.GetFileNameWithoutExtension(path);

                StreamWriter file = new StreamWriter(outpath, false);
                file.WriteLine("model " + modelName);
                file.WriteLine("{");
                file.WriteLine("\tmesh = \"" + path + "\"");

                bool unlit = (comboBoxShaderType.SelectedIndex == 0);
                foreach(var material in m_scene.Materials)
                {
                    file.WriteLine("\tmaterial " + comboBoxShaderType.SelectedItem.ToString());
                    file.WriteLine("\t{");

                    string textureName = "";
                    if (material.HasTextureDiffuse)
                    {
                        var texture = m_scene.Textures[material.TextureDiffuse.TextureIndex];
                        textureName = texture.ToString();

                        //TODO wrapping
                        //Assimp.TextureWrapMode.Wrap;
                        
                        file.WriteLine("\t\tdiffuse = \"" + textureName + "_mask.png\"");

                        if (!unlit)
                        {
                            //don't ignore mask and normal maps
                            if (material.HasTextureDiffuse)
                            {
                                file.WriteLine("\t\tmask = \"" + textureName + "_mask.png\"");
                                if (checkBoxBump.Checked)
                                {
                                    file.WriteLine("\t\tnormal = \"" + textureName + "_normal.png\"");
                                }
                            }
                        }
                        
                    }

                    if (material.HasColorDiffuse)
                    {
                        file.WriteLine("\t\tcolour = " + colourToString(material.ColorDiffuse));
                        if(!unlit)
                        {
                            float intensity = 1f;
                            float specAmount = 1f;
                            float emissive = 0f;

                            if (material.HasShininessStrength)
                            {
                                intensity = material.ShininessStrength; //TODO clamp 0-1
                            }

                            if (material.HasColorSpecular)
                            {
                                specAmount = (material.ColorSpecular.R + material.ColorSpecular.G + material.ColorSpecular.B) / 3f;
                            }

                            if(material.HasColorEmissive)
                            {
                                emissive = (material.ColorEmissive.R + material.ColorEmissive.G + material.ColorEmissive.B) / 3f;
                            }

                            file.WriteLine("\t\tmask_colour = "+ intensity.ToString() +"," + specAmount.ToString() + "," + emissive.ToString() +",1.0");
                        }
                    }

                    //blendmodes
                    if(material.HasBlendMode)
                    {
                        switch(material.BlendMode)
                        {
                            case Assimp.BlendMode.Additive:
                                file.WriteLine("\t\tblendmode = add");
                                break;
                            default:
                                if(material.Opacity < 1)
                                {
                                    file.WriteLine("\t\tblendmode = alpha");
                                }
                                break;
                        }
                    }

                    //lightmap
                    if(m_scene.Meshes[0].HasTextureCoords(1))
                    {
                        file.WriteLine("\t\tlightmap = " + modelName + "_lightmap.png");
                    }

                    file.WriteLine("\t}");
                }

                file.WriteLine("}");
                file.Close();
            }
            catch(Exception ex)
            {
                MessageBox.Show("Failed writing cmt file: " + ex.Message);
            }
        }
    }
}
